// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/tap_device_builder.h"

#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <utility>

#include <base/logging.h>

namespace vm_tools {
namespace concierge {
namespace {

// Path to the tun device.
constexpr char kTunDev[] = "/dev/net/tun";

// Format for the interface name.
constexpr char kInterfaceNameFormat[] = "vmtap%d";

// Size of the vnet header.
constexpr int32_t kVnetHeaderSize = 12;

}  // namespace

base::ScopedFD BuildTapDevice(const arc_networkd::MacAddress& mac_addr,
                              uint32_t ipv4_addr,
                              uint32_t ipv4_netmask) {
  // Explicitly not opened with close-on-exec because we want this fd to be
  // inherited by the child process.
  base::ScopedFD dev(open(kTunDev, O_RDWR | O_NONBLOCK));

  if (!dev.is_valid()) {
    PLOG(ERROR) << "Failed to open " << kTunDev;
    return dev;
  }

  // Create the interface.
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, kInterfaceNameFormat, sizeof(ifr.ifr_name));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_VNET_HDR;

  // This will overwrite the ifr_name field with the actual name of the
  // interface.
  if (ioctl(dev.get(), TUNSETIFF, &ifr) != 0) {
    PLOG(ERROR) << "Failed to create tun interface";
    return base::ScopedFD();
  }

  // Create the socket for configuring the interface.
  base::ScopedFD sock(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
  if (!sock.is_valid()) {
    PLOG(ERROR) << "Unable to create datagram socket";
    return base::ScopedFD();
  }

  // Set the ip address.
  struct sockaddr_in* addr =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = static_cast<in_addr_t>(ipv4_addr);
  if (ioctl(sock.get(), SIOCSIFADDR, &ifr) != 0) {
    PLOG(ERROR) << "Failed to set ip address for vmtap interface";
    return base::ScopedFD();
  }

  // Set the netmask.
  struct sockaddr_in* netmask =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_netmask);
  netmask->sin_family = AF_INET;
  netmask->sin_addr.s_addr = static_cast<in_addr_t>(ipv4_netmask);
  if (ioctl(sock.get(), SIOCSIFNETMASK, &ifr) != 0) {
    PLOG(ERROR) << "Failed to set netmask for vmtap interface";
    return base::ScopedFD();
  }

  // Set the mac address.
  struct sockaddr* hwaddr = &ifr.ifr_hwaddr;
  hwaddr->sa_family = ARPHRD_ETHER;
  memcpy(&hwaddr->sa_data, &mac_addr, sizeof(mac_addr));
  if (ioctl(sock.get(), SIOCSIFHWADDR, &ifr) != 0) {
    PLOG(ERROR) << "Failed to set mac address for vmtap interface";
    return base::ScopedFD();
  }

  // Set the vnet header size.
  if (ioctl(dev.get(), TUNSETVNETHDRSZ, &kVnetHeaderSize) != 0) {
    PLOG(ERROR) << "Failed to set vnet header size for vmtap interface";
    return base::ScopedFD();
  }

  // Set the offload flags.  These must match the virtio features advertised by
  // the net device in crosvm.
  if (ioctl(dev.get(), TUNSETOFFLOAD,
            TUN_F_CSUM | TUN_F_UFO | TUN_F_TSO4 | TUN_F_TSO6) != 0) {
    PLOG(ERROR) << "Failed to set offload for vmtap interface";
    return base::ScopedFD();
  }

  // Finally, enable the device.
  if (ioctl(sock.get(), SIOCGIFFLAGS, &ifr) != 0) {
    PLOG(ERROR) << "Failed to get flags for vmtap interface";
    return base::ScopedFD();
  }

  ifr.ifr_flags = IFF_UP | IFF_RUNNING;
  if (ioctl(sock.get(), SIOCSIFFLAGS, &ifr) != 0) {
    PLOG(ERROR) << "Failed to enable vmtap interface";
    return base::ScopedFD();
  }

  return dev;
}

}  // namespace concierge
}  // namespace vm_tools
