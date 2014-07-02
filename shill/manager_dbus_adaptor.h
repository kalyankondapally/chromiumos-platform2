// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_DBUS_ADAPTOR_H_
#define SHILL_MANAGER_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.Manager.h"

namespace shill {

class Manager;

// Subclass of DBusAdaptor for Manager objects
// There is a 1:1 mapping between Manager and ManagerDBusAdaptor
// instances.  Furthermore, the Manager owns the ManagerDBusAdaptor
// and manages its lifetime, so we're OK with ManagerDBusAdaptor
// having a bare pointer to its owner manager.
class ManagerDBusAdaptor : public org::chromium::flimflam::Manager_adaptor,
                           public DBusAdaptor,
                           public ManagerAdaptorInterface,
                           public base::SupportsWeakPtr<ManagerDBusAdaptor> {
 public:
  static const char kPath[];

  ManagerDBusAdaptor(DBus::Connection *conn, Manager *manager);
  virtual ~ManagerDBusAdaptor();

  // Implementation of ManagerAdaptorInterface.
  virtual const std::string &GetRpcIdentifier() { return path(); }
  void UpdateRunning();
  void EmitBoolChanged(const std::string &name, bool value);
  void EmitUintChanged(const std::string &name, uint32 value);
  void EmitIntChanged(const std::string &name, int value);
  void EmitStringChanged(const std::string &name, const std::string &value);
  void EmitStringsChanged(const std::string &name,
                          const std::vector<std::string> &value);
  void EmitRpcIdentifierChanged(
      const std::string &name, const std::string &value);
  void EmitRpcIdentifierArrayChanged(
      const std::string &name, const std::vector<std::string> &value);
  void EmitStateChanged(const std::string &new_state);

  // Implementation of Manager_adaptor
  std::map<std::string, ::DBus::Variant> GetProperties(::DBus::Error &error);
  void SetProperty(const std::string &name,
                   const ::DBus::Variant &value,
                   ::DBus::Error &error);
  std::string GetState(::DBus::Error &error);
  ::DBus::Path CreateProfile(const std::string &name, ::DBus::Error &error);
  void RemoveProfile(const std::string &name, ::DBus::Error &error);
  ::DBus::Path PushProfile(const std::string &, ::DBus::Error &error);
  ::DBus::Path InsertUserProfile(const std::string &,
                                 const std::string &,
                                 ::DBus::Error &error);
  void PopProfile(const std::string &, ::DBus::Error &error);
  void PopAnyProfile(::DBus::Error &error);
  void PopAllUserProfiles(::DBus::Error &error);
  void RecheckPortal(::DBus::Error &error);
  void RequestScan(const std::string &, ::DBus::Error &error);

  void EnableTechnology(const std::string &, ::DBus::Error &error);
  void DisableTechnology(const std::string &, ::DBus::Error &error);

  ::DBus::Path GetService(const std::map<std::string, ::DBus::Variant> &args,
                          ::DBus::Error &error);
  ::DBus::Path GetVPNService(const std::map<std::string, ::DBus::Variant> &args,
                             ::DBus::Error &error);
  ::DBus::Path GetWifiService(
      const std::map<std::string, ::DBus::Variant> &args,
      ::DBus::Error &error);
  ::DBus::Path ConfigureService(
      const std::map<std::string, ::DBus::Variant> &args,
      ::DBus::Error &error);
  ::DBus::Path ConfigureServiceForProfile(
      const ::DBus::Path &profile_rpcid,
      const std::map<std::string, ::DBus::Variant> &args,
      ::DBus::Error &error);
  ::DBus::Path FindMatchingService(
      const std::map<std::string, ::DBus::Variant> &args,
      ::DBus::Error &error);

  int32_t GetDebugLevel(::DBus::Error &error);
  void SetDebugLevel(const int32_t &level, ::DBus::Error &error);

  std::string GetServiceOrder(::DBus::Error &error);
  void SetServiceOrder(const std::string &, ::DBus::Error &error);

  std::string GetDebugTags(::DBus::Error &error);
  void SetDebugTags(const std::string &tags, ::DBus::Error &error);
  std::string ListDebugTags(::DBus::Error &error);

  std::map<std::string, ::DBus::Variant> GetNetworksForGeolocation(
      ::DBus::Error &error);

  bool VerifyDestination(const std::string &certificate,
                         const std::string &public_key,
                         const std::string &nonce,
                         const std::string &signed_data,
                         const std::string &destination_udn,
                         const std::string &hotspot_ssid,
                         const std::string &hotspot_bssid,
                         ::DBus::Error &error);

  std::string VerifyAndEncryptCredentials(const std::string &certificate,
                                          const std::string &public_key,
                                          const std::string &nonce,
                                          const std::string &signed_data,
                                          const std::string &destination_udn,
                                          const std::string &hotspot_ssid,
                                          const std::string &hotspot_bssid,
                                          const ::DBus::Path &path,
                                          ::DBus::Error &error);

  std::string VerifyAndEncryptData(const std::string &certificate,
                                   const std::string &public_key,
                                   const std::string &nonce,
                                   const std::string &signed_data,
                                   const std::string &destination_udn,
                                   const std::string &hotspot_ssid,
                                   const std::string &hotspot_bssid,
                                   const std::string &data,
                                   ::DBus::Error &error);

  void ConnectToBestServices(::DBus::Error &error);

 private:
  Manager *manager_;
  DISALLOW_COPY_AND_ASSIGN(ManagerDBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_MANAGER_DBUS_ADAPTOR_H_
