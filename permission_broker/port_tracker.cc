// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/port_tracker.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <ostream>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_util.h>

namespace permission_broker {

namespace {
constexpr const int kMaxEvents = 10;
constexpr const int64_t kLifelineIntervalSeconds = 5;
constexpr const int kInvalidHandle = -1;
// Port forwarding is only allowed for non-reserved ports.
constexpr const uint16_t kLastSystemPort = 1023;
// Port forwarding is only allowed for some physical interfaces: Ethernet, USB
// tethering, and WiFi.
constexpr std::array<const char*, 4> kAllowedInterfacePrefixes{
    {"eth", "usb", "wlan", "mlan"}};
constexpr const char kLocalhost[] = "lo";

// Returns the network-byte order int32 representation of the IPv4 address given
// byte per byte, most significant bytes first.
constexpr uint32_t Ipv4Addr(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

// TODO(hugobenichi): eventually import these values from
// platform2/arc/network.
// Port forwarding can only forwards to IPv4 addresses within the subnet used
// for static IPv4 assignement to guest OSs and app platforms.
constexpr const char* kGuestSubnetCidr = "100.115.92.0/24";
constexpr const struct in_addr kGuestBaseAddr = {.s_addr =
                                                     Ipv4Addr(100, 115, 92, 0)};
constexpr const struct in_addr kGuestNetmask = {.s_addr =
                                                    Ipv4Addr(255, 255, 255, 0)};

std::ostream& operator<<(std::ostream& stream,
                         const PortTracker::PortRuleKey key) {
  stream << "{ " << ProtocolName(key.proto) << " :"
         << std::to_string(key.input_dst_port) << "/" << key.input_ifname
         << " }";
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const PortTracker::PortRule rule) {
  stream << "{ " << ProtocolName(rule.proto) << " :"
         << std::to_string(rule.input_dst_port) << "/" << rule.input_ifname
         << " -> " << rule.dst_ip << ":" << rule.dst_port << " }";
  return stream;
}
}  // namespace

PortTracker::PortTracker(Firewall* firewall)
    : task_runner_{base::MessageLoopForIO::current()->task_runner()},
      epfd_{kInvalidHandle},
      firewall_{firewall} {}

// Test-only.
PortTracker::PortTracker(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         Firewall* firewall)
    : task_runner_{task_runner}, epfd_{kInvalidHandle}, firewall_{firewall} {}

PortTracker::~PortTracker() {
  RevokeAllPortRules();

  if (epfd_ >= 0) {
    close(epfd_);
  }
}

bool PortTracker::AllowTcpPortAccess(uint16_t port,
                                     const std::string& iface,
                                     int dbus_fd) {
  PortRuleKey key = {
      .proto = kProtocolTcp,
      .input_dst_port = port,
      .input_ifname = iface,
  };
  return OpenPort(key, dbus_fd);
}

bool PortTracker::AllowUdpPortAccess(uint16_t port,
                                     const std::string& iface,
                                     int dbus_fd) {
  PortRuleKey key = {
      .proto = kProtocolUdp,
      .input_dst_port = port,
      .input_ifname = iface,
  };
  return OpenPort(key, dbus_fd);
}

bool PortTracker::RevokeTcpPortAccess(uint16_t port, const std::string& iface) {
  PortRuleKey key = {
      .proto = kProtocolTcp,
      .input_dst_port = port,
      .input_ifname = iface,
  };
  return ClosePort(key);
}

bool PortTracker::RevokeUdpPortAccess(uint16_t port, const std::string& iface) {
  PortRuleKey key = {
      .proto = kProtocolUdp,
      .input_dst_port = port,
      .input_ifname = iface,
  };
  return ClosePort(key);
}

bool PortTracker::OpenPort(const PortRuleKey& key, int dbus_fd) {
  if (open_port_fds_.find(key) != open_port_fds_.end()) {
    // This can happen when a requesting process has just been restarted but
    // the scheduled lifeline FD check hasn't yet been performed, so we might
    // have stale file descriptors around.
    // Force the FD check to see if they will be removed now.
    CheckLifelineFds(false /* reschedule_check */);

    // Then try again. If this still fails, we know it's an invalid request.
    if (open_port_fds_.find(key) != open_port_fds_.end()) {
      LOG(ERROR) << "Hole already punched for " << key;
      return false;
    }
  }

  // Check if the port is not already being forwarded.
  if (forwarding_rules_fds_.find(key) != forwarding_rules_fds_.end()) {
    // Remove stale lifeline fds and recheck.
    CheckLifelineFds(false /* reschedule_check */);

    if (forwarding_rules_fds_.find(key) != forwarding_rules_fds_.end()) {
      LOG(ERROR) << "Already forwarding " << key;
      return false;
    }
  }

  // We use |lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for port " << key << " failed";
    return false;
  }

  // Track the port rule.
  lifeline_fds_[lifeline_fd] = key;
  open_port_fds_[key] = lifeline_fd;

  bool success = firewall_->AddAcceptRules(key.proto, key.input_dst_port,
                                           key.input_ifname);
  if (!success) {
    // If we fail to punch the hole in the firewall, stop tracking the lifetime
    // of the process.
    LOG(ERROR) << "Failed to punch hole for port " << key;
    DeleteLifelineFd(lifeline_fd);
    lifeline_fds_.erase(lifeline_fd);
    open_port_fds_.erase(key);
    return false;
  }
  return true;
}

bool PortTracker::ClosePort(const PortRuleKey& key) {
  auto p = open_port_fds_.find(key);
  if (p == open_port_fds_.end()) {
    LOG(ERROR) << "Not tracking port " << key;
    return false;
  }

  int fd = p->second;
  bool plugged = firewall_->DeleteAcceptRules(key.proto, key.input_dst_port,
                                              key.input_ifname);
  bool deleted = DeleteLifelineFd(fd);
  lifeline_fds_.erase(fd);
  open_port_fds_.erase(key);
  if (!plugged) {
    LOG(ERROR) << "Failed to close open port " << key;
  }
  if (!deleted) {
    LOG(ERROR) << "Failed to delete file descriptor " << fd
               << " from epoll instance";
  }
  return plugged && deleted;
}

void PortTracker::RevokeAllPortRules() {
  VLOG(1) << "Revoking all port rules";

  // Copy the container so that we can remove elements from the original.
  std::vector<PortRuleKey> all_rules;
  all_rules.reserve(lifeline_fds_.size());
  for (const auto& kv : lifeline_fds_) {
    all_rules.push_back(kv.second);
  }
  for (const PortRuleKey& key : all_rules) {
    RevokePortRule(key);
  }

  CHECK(lifeline_fds_.empty()) << "Failed to revoke all port rules";
}

bool PortTracker::LockDownLoopbackTcpPort(uint16_t port, int dbus_fd) {
  PortRuleKey key = {
      .proto = kProtocolTcp,
      .input_dst_port = port,
      .input_ifname = kLocalhost,
  };
  if (tcp_loopback_fds_.find(key) != tcp_loopback_fds_.end()) {
    // This can happen when a requesting process has just been restarted but
    // the scheduled lifeline FD check hasn't yet been performed, so we might
    // have stale file descriptors around.
    // Force the FD check to see if they will be removed now.
    CheckLifelineFds(false /* reschedule_check */);

    // Then try again. If this still fails, we know it's an invalid request.
    if (tcp_loopback_fds_.find(key) != tcp_loopback_fds_.end()) {
      LOG(ERROR) << "Port " << key << " already locked down";
      return false;
    }
  }

  // We use |lifeline_fd| to track the lifetime of the process requesting
  // port access.
  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for port " << key << " failed";
    return false;
  }

  // Track the port.
  lifeline_fds_[lifeline_fd] = key;
  tcp_loopback_fds_[key] = lifeline_fd;

  bool success =
      firewall_->AddLoopbackLockdownRules(key.proto, key.input_dst_port);
  if (!success) {
    // If we fail to lock down the port in the firewall, stop tracking the
    // lifetime of the process.
    LOG(ERROR) << "Failed to lock down port " << key;
    DeleteLifelineFd(lifeline_fd);
    lifeline_fds_.erase(lifeline_fd);
    tcp_loopback_fds_.erase(key);
    return false;
  }
  return true;
}

bool PortTracker::ReleaseLoopbackTcpPort(uint16_t port) {
  PortRuleKey key = {
      .proto = kProtocolTcp,
      .input_dst_port = port,
      .input_ifname = kLocalhost,
  };
  return ReleaseLoopbackTcpPortInternal(key);
}

bool PortTracker::ReleaseLoopbackTcpPortInternal(const PortRuleKey& key) {
  auto p = tcp_loopback_fds_.find(key);
  if (p == tcp_loopback_fds_.end()) {
    LOG(ERROR) << "Not tracking port " << key;
    return false;
  }

  int fd = p->second;
  bool plugged =
      firewall_->DeleteLoopbackLockdownRules(key.proto, key.input_dst_port);
  bool deleted = DeleteLifelineFd(fd);
  lifeline_fds_.erase(fd);
  tcp_loopback_fds_.erase(key);
  if (!plugged) {
    LOG(ERROR) << "Failed to delete loopback lockdown rule for port " << key;
  }
  if (!deleted) {
    LOG(ERROR) << "Failed to delete file descriptor " << fd
               << " from epoll instance";
  }
  return plugged && deleted;
}

bool PortTracker::StartTcpPortForwarding(uint16_t input_dst_port,
                                         const std::string& input_ifname,
                                         const std::string& dst_ip,
                                         uint16_t dst_port,
                                         int dbus_fd) {
  PortRule rule = {
      .proto = kProtocolTcp,
      .input_dst_port = input_dst_port,
      .input_ifname = input_ifname,
      .dst_ip = dst_ip,
      .dst_port = dst_port,
  };
  return AddForwardingRule(rule, dbus_fd);
}

bool PortTracker::StartUdpPortForwarding(uint16_t input_dst_port,
                                         const std::string& input_ifname,
                                         const std::string& dst_ip,
                                         uint16_t dst_port,
                                         int dbus_fd) {
  PortRule rule = {
      .proto = kProtocolUdp,
      .input_dst_port = input_dst_port,
      .input_ifname = input_ifname,
      .dst_ip = dst_ip,
      .dst_port = dst_port,
  };
  return AddForwardingRule(rule, dbus_fd);
}

bool PortTracker::StopTcpPortForwarding(uint16_t input_dst_port,
                                        const std::string& input_ifname) {
  PortRuleKey key = {
      .proto = kProtocolTcp,
      .input_dst_port = input_dst_port,
      .input_ifname = input_ifname,
  };
  return RemoveForwardingRule(key);
}

bool PortTracker::StopUdpPortForwarding(uint16_t input_dst_port,
                                        const std::string& input_ifname) {
  PortRuleKey key = {
      .proto = kProtocolUdp,
      .input_dst_port = input_dst_port,
      .input_ifname = input_ifname,
  };
  return RemoveForwardingRule(key);
}

bool PortTracker::AddForwardingRule(const PortRule& rule, int dbus_fd) {
  switch (rule.proto) {
    case kProtocolTcp:
    case kProtocolUdp:
      break;
    default:
      CHECK(false) << "Unexpected L4 protocol value " << rule.proto;
      return false;
  }

  // Redirecting a reserved port is not allowed.
  // Forwarding into a reserved port of the guest is allowed.
  if (rule.input_dst_port <= kLastSystemPort) {
    LOG(ERROR) << "Cannot forward system port " << rule.input_dst_port;
    return false;
  }

  struct in_addr addr;
  if (inet_pton(AF_INET, rule.dst_ip.c_str(), &addr) != 1) {
    LOG(ERROR) << "Cannot forward to invalid IPv4 address " << rule.dst_ip;
    return false;
  }

  if ((addr.s_addr & kGuestNetmask.s_addr) != kGuestBaseAddr.s_addr) {
    LOG(ERROR) << "Cannot forward to IPv4 address " << rule.dst_ip
               << " outside of " << kGuestSubnetCidr;
    return false;
  }

  if (rule.input_ifname.empty()) {
    PLOG(ERROR) << "No interface name provided";
    return false;
  }

  bool allowedInputIface = false;
  for (const auto& prefix : kAllowedInterfacePrefixes) {
    if (base::StartsWith(rule.input_ifname, prefix,
                         base::CompareCase::SENSITIVE)) {
      allowedInputIface = true;
      break;
    }
  }
  if (!allowedInputIface) {
    PLOG(ERROR) << "Cannot forward traffic from interface "
                << rule.input_ifname;
    return false;
  }

  PortRuleKey key = {
      .proto = rule.proto,
      .input_dst_port = rule.input_dst_port,
      .input_ifname = rule.input_ifname,
  };

  // Check if the port is not already open for ingress traffic.
  if (open_port_fds_.find(key) != open_port_fds_.end()) {
    // Remove stale lifeline fds and recheck.
    CheckLifelineFds(false /* reschedule_check */);
    if (open_port_fds_.find(key) != open_port_fds_.end()) {
      LOG(ERROR) << "Cannot apply forwarding rule " << rule
                 << ": port is already open for ingress traffic";
      return false;
    }
  }

  // Check if the port is not already forwarded.
  if (forwarding_rules_fds_.find(key) != forwarding_rules_fds_.end()) {
    // Remove stale lifeline fds and recheck.
     /* reschedule_check */CheckLifelineFds(false /* reschedule_check */);

    if (forwarding_rules_fds_.find(key) != forwarding_rules_fds_.end()) {
      LOG(ERROR) << "Forwarding rule already exist for " << key;
      return false;
    }
  }

  int lifeline_fd = AddLifelineFd(dbus_fd);
  if (lifeline_fd < 0) {
    LOG(ERROR) << "Tracking lifeline fd for forwarding rule " << rule
               << " failed";
    return false;
  }

  forwarding_rules_fds_[key] = rule;
  forwarding_rules_fds_[key].lifeline_fd = lifeline_fd;
  lifeline_fds_[lifeline_fd] = key;

  bool success = firewall_->DeleteIpv4ForwardRule(
      rule.proto, rule.input_dst_port, rule.input_ifname, rule.dst_ip,
      rule.dst_port);
  if (!success) {
    LOG(ERROR) << "Failed to delete forwarding rule " << rule;
    DeleteLifelineFd(lifeline_fd);
    forwarding_rules_fds_.erase(key);
    lifeline_fds_.erase(lifeline_fd);
    return false;
  }

  VLOG(1) << "Added port forwarding rule " << rule;
  return true;
}

bool PortTracker::RemoveForwardingRule(const PortRuleKey& key) {
  if (forwarding_rules_fds_.find(key) == forwarding_rules_fds_.end()) {
    LOG(ERROR) << "No forwarding rule found for " << key;
    return false;
  }

  if (forwarding_rules_fds_.find(key) == forwarding_rules_fds_.end()) {
    LOG(ERROR) << "No file descriptor associated with forwarding rule " << key;
    return false;
  }

  PortRule rule = forwarding_rules_fds_[key];

  DeleteLifelineFd(rule.lifeline_fd);
  forwarding_rules_fds_.erase(key);
  lifeline_fds_.erase(rule.lifeline_fd);

  bool success = firewall_->DeleteIpv4ForwardRule(
      rule.proto, rule.input_dst_port, rule.input_ifname, rule.dst_ip,
      rule.dst_port);
  if (!success) {
    LOG(ERROR) << "Failed to remove forwarding rule " << rule;
    return false;
  }

  VLOG(1) << "Removed port forwarding rule " << rule;
  return true;
}

int PortTracker::AddLifelineFd(int dbus_fd) {
  if (!InitializeEpollOnce()) {
    return kInvalidHandle;
  }
  int fd = dup(dbus_fd);

  struct epoll_event epevent;
  epevent.events = EPOLLIN;  // EPOLLERR | EPOLLHUP are always waited for.
  epevent.data.fd = fd;
  VLOG(1) << "Adding file descriptor " << fd << " to epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &epevent) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_ADD)";
    return kInvalidHandle;
  }

  // If this is the first port request, start lifeline checks.
  if (HasActiveRules()) {
    VLOG(1) << "Starting lifeline checks";
    ScheduleLifelineCheck();
  }

  return fd;
}

bool PortTracker::DeleteLifelineFd(int fd) {
  if (epfd_ < 0) {
    LOG(ERROR) << "epoll instance not created";
    return false;
  }

  VLOG(1) << "Deleting file descriptor " << fd << " from epoll instance";
  if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) != 0) {
    PLOG(ERROR) << "epoll_ctl(EPOLL_CTL_DEL)";
    return false;
  }

  // AddLifelineFd() calls dup(), so this function should close the fd.
  // We still return true since at this point the fd has been deleted from the
  // epoll instance.
  if (IGNORE_EINTR(close(fd)) < 0) {
    PLOG(ERROR) << "close(lifeline_fd)";
  }
  return true;
}

void PortTracker::CheckLifelineFds(bool reschedule_check) {
  if (epfd_ < 0) {
    return;
  }
  struct epoll_event epevents[kMaxEvents];
  int nready = epoll_wait(epfd_, epevents, kMaxEvents, 0 /* do not block */);
  if (nready < 0) {
    PLOG(ERROR) << "epoll_wait(0)";
    return;
  }
  if (nready == 0) {
    if (reschedule_check)
      ScheduleLifelineCheck();
    return;
  }

  for (size_t eidx = 0; eidx < (size_t)nready; eidx++) {
    uint32_t events = epevents[eidx].events;
    int fd = epevents[eidx].data.fd;
    if ((events & (EPOLLHUP | EPOLLERR))) {
      // The process that requested this port has died/exited,
      // so we need to plug the hole.
      if (lifeline_fds_.find(fd) == lifeline_fds_.end()) {
        LOG(ERROR) << "File descriptor " << fd << " was not being tracked";
        DeleteLifelineFd(fd);
        continue;
      }
      if (!RevokePortRule(lifeline_fds_[fd])) {
        DeleteLifelineFd(fd);
      }
    }
  }

  if (reschedule_check) {
    // If there's still processes to track, schedule lifeline checks.
    if (HasActiveRules()) {
      ScheduleLifelineCheck();
    } else {
      VLOG(1) << "Stopping lifeline checks";
    }
  }
}

void PortTracker::ScheduleLifelineCheck() {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PortTracker::CheckLifelineFds, base::Unretained(this), true),
      base::TimeDelta::FromSeconds(kLifelineIntervalSeconds));
}

bool PortTracker::HasActiveRules() {
  return !lifeline_fds_.empty();
}

bool PortTracker::RevokePortRule(const PortRuleKey& key) {
  if (open_port_fds_.find(key) != open_port_fds_.end()) {
    // It was a port accept rule.
    return ClosePort(key);
  } else if (tcp_loopback_fds_.find(key) != tcp_loopback_fds_.end()) {
    // It was a blocked TCP loopback port.
    return ReleaseLoopbackTcpPortInternal(key);
  } else if (forwarding_rules_fds_.find(key) != forwarding_rules_fds_.end()) {
    // It was a forwarding rule.
    return RemoveForwardingRule(key);
  } else {
    LOG(ERROR) << "Unknown port rule entry " << key;
    return false;
  }
}

bool PortTracker::InitializeEpollOnce() {
  if (epfd_ < 0) {
    // |size| needs to be > 0, but is otherwise ignored.
    VLOG(1) << "Creating epoll instance";
    epfd_ = epoll_create(1 /* size */);
    if (epfd_ < 0) {
      PLOG(ERROR) << "epoll_create()";
      return false;
    }
  }
  return true;
}

}  // namespace permission_broker
