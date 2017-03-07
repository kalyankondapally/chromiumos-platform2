// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIOMETRICS_DAEMON_H_
#define BIOD_BIOMETRICS_DAEMON_H_

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <brillo/dbus/exported_object_manager.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "biod/biometrics_manager.h"

namespace biod {

class BiometricsManagerWrapper {
 public:
  BiometricsManagerWrapper(
      std::unique_ptr<BiometricsManager> biometrics_manager,
      brillo::dbus_utils::ExportedObjectManager* object_manager,
      dbus::ObjectPath object_path,
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

  BiometricsManager& get() {
    DCHECK(biometrics_manager_);
    return *biometrics_manager_.get();
  }

  // Updates the list of records reflected as dbus objects.
  void RefreshRecordObjects();

 private:
  class RecordWrapper {
   public:
    RecordWrapper(BiometricsManagerWrapper* biometrics_manager,
                  std::unique_ptr<BiometricsManager::Record> record,
                  brillo::dbus_utils::ExportedObjectManager* object_manager,
                  const dbus::ObjectPath& object_path);
    ~RecordWrapper();

    const dbus::ObjectPath& path() const { return object_path_; }

    const std::string& GetUserId() const { return record_->GetUserId(); }

   private:
    BiometricsManagerWrapper* biometrics_manager_;
    std::unique_ptr<BiometricsManager::Record> record_;
    brillo::dbus_utils::DBusObject dbus_object_;
    dbus::ObjectPath object_path_;
    brillo::dbus_utils::ExportedProperty<std::string> property_label_;

    bool SetLabel(brillo::ErrorPtr* error, const std::string& new_label);
    bool Remove(brillo::ErrorPtr* error);

    DISALLOW_COPY_AND_ASSIGN(RecordWrapper);
  };

  std::unique_ptr<BiometricsManager> biometrics_manager_;

  brillo::dbus_utils::DBusObject dbus_object_;
  dbus::ObjectPath object_path_;
  brillo::dbus_utils::ExportedProperty<uint32_t> property_type_;
  std::vector<std::unique_ptr<RecordWrapper>> records_;

  BiometricsManager::EnrollSession enroll_session_;
  std::string enroll_session_owner_;
  dbus::ObjectPath enroll_session_object_path_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> enroll_session_dbus_object_;

  BiometricsManager::AuthSession auth_session_;
  std::string auth_session_owner_;
  dbus::ObjectPath auth_session_object_path_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> auth_session_dbus_object_;

  void FinalizeEnrollSessionObject();
  void FinalizeAuthSessionObject();

  void OnNameOwnerChanged(dbus::Signal* signal);

  void OnEnrollScanDone(BiometricsManager::ScanResult scan_result, bool done);
  void OnAuthScanDone(BiometricsManager::ScanResult scan_result,
                      BiometricsManager::AttemptMatches matches);
  void OnSessionFailed();

  bool StartEnrollSession(brillo::ErrorPtr* error,
                          dbus::Message* message,
                          const std::string& user_id,
                          const std::string& label,
                          dbus::ObjectPath* enroll_session_path);
  bool GetRecordsForUser(brillo::ErrorPtr* error,
                         const std::string& user_id,
                         std::vector<dbus::ObjectPath>* out);
  bool DestroyAllRecords(brillo::ErrorPtr* error);
  bool StartAuthSession(brillo::ErrorPtr* error,
                        dbus::Message* message,
                        dbus::ObjectPath* auth_session_path);

  bool EnrollSessionCancel(brillo::ErrorPtr* error);

  bool AuthSessionEnd(brillo::ErrorPtr* error);

  DISALLOW_COPY_AND_ASSIGN(BiometricsManagerWrapper);
};

class BiometricsDaemon {
 public:
  BiometricsDaemon();

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<brillo::dbus_utils::ExportedObjectManager> object_manager_;
  std::vector<std::unique_ptr<BiometricsManagerWrapper>> biometrics_managers_;

  // Proxy for dbus communication with session manager / login.
  scoped_refptr<dbus::ObjectProxy> session_manager_proxy_;
  // Keep track of currently logged in users.
  std::unordered_set<std::string> current_active_users_;

  // Gets the set of active users since the last time this method was called.
  bool RetrieveNewActiveSessions(
      std::unordered_set<std::string>* new_active_users);

  // Read or delete records in memory when users log in or out.
  void OnSessionStateChanged(dbus::Signal* signal);

  DISALLOW_COPY_AND_ASSIGN(BiometricsDaemon);
};
}  // namespace biod

#endif  // BIOD_BIOMETRICS_DAEMON_H_
