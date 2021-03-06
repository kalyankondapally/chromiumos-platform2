// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PATCHPANEL_DBUS_CLIENT_H_
#define PATCHPANEL_DBUS_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include <brillo/brillo_export.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>
#include <patchpanel/proto_bindings/patchpanel_service.pb.h>

namespace patchpanel {

// Simple wrapper around patchpanel DBus API. All public functions are blocking
// DBus calls to patchpaneld (asynchronous calls are mentioned explicitly). The
// method names and protobuf schema used by patchpanel DBus API are defined in
// platform2/system_api/dbus/patchpanel. Access control for clients is defined
// in platform2/patchpanel/dbus.
class BRILLO_EXPORT Client {
 public:
  using GetTrafficCountersCallback =
      base::OnceCallback<void(const std::vector<TrafficCounter>&)>;
  using NeighborConnectedStateChangedHandler =
      base::RepeatingCallback<void(const NeighborConnectedStateChangedSignal&)>;

  static std::unique_ptr<Client> New();

  // Only used in tests.
  static std::unique_ptr<Client> New(const scoped_refptr<dbus::Bus>& bus,
                                     dbus::ObjectProxy* proxy);

  virtual ~Client() = default;

  virtual bool NotifyArcStartup(pid_t pid) = 0;
  virtual bool NotifyArcShutdown() = 0;

  virtual std::vector<NetworkDevice> NotifyArcVmStartup(uint32_t cid) = 0;
  virtual bool NotifyArcVmShutdown(uint32_t cid) = 0;

  virtual bool NotifyTerminaVmStartup(uint32_t cid,
                                      NetworkDevice* device,
                                      IPv4Subnet* container_subnet) = 0;
  virtual bool NotifyTerminaVmShutdown(uint32_t cid) = 0;

  virtual bool NotifyPluginVmStartup(uint64_t vm_id,
                                     int subnet_index,
                                     NetworkDevice* device) = 0;
  virtual bool NotifyPluginVmShutdown(uint64_t vm_id) = 0;

  // Reset the VPN routing intent mark on a socket to the default policy for
  // the current uid. This is in general incorrect to call this method for
  // a socket that is already connected.
  virtual bool DefaultVpnRouting(int socket) = 0;

  // Mark a socket to be always routed through a VPN if there is one.
  // Must be called before the socket is connected.
  virtual bool RouteOnVpn(int socket) = 0;

  // Mark a socket to be always routed through the physical network.
  // Must be called before the socket is connected.
  virtual bool BypassVpn(int socket) = 0;

  // Sends a ConnectNamespaceRequest for the given namespace pid. Returns a
  // pair with a valid ScopedFD and the ConnectNamespaceResponse proto message
  // received if the request succeeded. Closing the ScopedFD will teardown the
  // veth and routing setup and free the allocated IPv4 subnet.
  virtual std::pair<base::ScopedFD, patchpanel::ConnectNamespaceResponse>
  ConnectNamespace(pid_t pid,
                   const std::string& outbound_ifname,
                   bool forward_user_traffic) = 0;

  // Gets the traffic counters kept by patchpanel asynchronously, |callback|
  // will be called with the counters once they are ready, or with an empty
  // vector when an error happen. |devices| is the set of interfaces (shill
  // devices) for which counters should be returned, any unknown interfaces will
  // be ignored. If |devices| is empty, counters for all known interfaces will
  // be returned.
  virtual void GetTrafficCounters(const std::set<std::string>& devices,
                                  GetTrafficCountersCallback callback) = 0;

  // Sends a ModifyPortRuleRequest to modify iptables ingress rules.
  // This should only be called by permission_broker's 'devbroker'.
  virtual bool ModifyPortRule(patchpanel::ModifyPortRuleRequest::Operation op,
                              patchpanel::ModifyPortRuleRequest::RuleType type,
                              patchpanel::ModifyPortRuleRequest::Protocol proto,
                              const std::string& input_ifname,
                              const std::string& input_dst_ip,
                              uint32_t input_dst_port,
                              const std::string& dst_ip,
                              uint32_t dst_port) = 0;

  // Registers a handler that will be called on receiving a signal of neighbor
  // connected state changed. Currently these events are generated only for WiFi
  // devices. The handler is registered for as long as this patchpanel::Client
  // instance is alive.
  virtual void RegisterNeighborConnectedStateChangedHandler(
      NeighborConnectedStateChangedHandler handler) = 0;

 protected:
  Client() = default;
};

}  // namespace patchpanel

#endif  // PATCHPANEL_DBUS_CLIENT_H_
