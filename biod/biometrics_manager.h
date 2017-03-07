// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIOMETRICS_MANAGER_H_
#define BIOD_BIOMETRICS_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>

namespace biod {

// A BiometricsManager object represents one biometric input device and all of
// the records registered with it. At a high level, there are 3 operations that
// are supported: 1) enrolling new record objects, 2) authenticating against
// those record objects, and 3) destroying all record objects made from this
// BiometricsManager. For DestroyAllRecords the operation is as simple as
// calling the function. For the other operations, the BiometricsManager object
// must be entered into AuthSession or EnrollSession, which is represented
// in code by the return of the session objects. EnrollSession and AuthSession
// can be thought of as session objects that are ongoing as long as the unique
// pointers remain in scope and the End/Cancel methods haven't been called. It's
// undefined what StartEnrollSession or StartAuthSession will do if there is an
// valid outstanding EnrollSession or AuthSession object in the wild.
class BiometricsManager {
 public:
  enum class Type {
    kFingerprint = 0,
    kRetina = 1,
    kFace = 2,
    kVoice = 3,
  };

  // Results of any type of any scan operation can fail due to user error. These
  // codes tell the user a little bit about what they did wrong, so they should
  // be conveyed to the user somehow after unsuccessful scan attempts.
  enum class ScanResult {
    kSuccess = 0,
    kPartial = 1,
    kInsufficient = 2,
    kSensorDirty = 3,
    kTooSlow = 4,
    kTooFast = 5,
    kImmobile = 6,
  };

  struct EnrollSessionEnder {
    void operator()(BiometricsManager* biometrics_manager) {
      biometrics_manager->EndEnrollSession();
    }
  };

  struct AuthSessionEnder {
    void operator()(BiometricsManager* biometrics_manager) {
      biometrics_manager->EndAuthSession();
    }
  };

  // Invokes the function object F with a given BiometricsManager object when
  // this session (EnrollSession or AuthSession) object goes out of scope. It's
  // possible that this will do nothing in the case that the session has ended
  // due to failure/finishing or the BiometricsManager object is no longer
  // valid.
  template <typename F>
  class Session {
   public:
    Session() = default;

    Session(Session<F>&& rhs) : biometrics_manager_(rhs.biometrics_manager_) {
      rhs.biometrics_manager_.reset();
    }

    explicit Session(const base::WeakPtr<BiometricsManager>& biometrics_manager)
        : biometrics_manager_(biometrics_manager) {}

    ~Session() { End(); }

    Session<F>& operator=(Session<F>&& rhs) {
      End();
      biometrics_manager_ = rhs.biometrics_manager_;
      rhs.biometrics_manager_.reset();
      return *this;
    }

    explicit operator bool() const { return biometrics_manager_; }

    // Has the same effect of letting this object go out of scope, but allows
    // one to reuse the storage of this object.
    void End() {
      if (biometrics_manager_) {
        F f;
        f(biometrics_manager_.get());
        biometrics_manager_.reset();
      }
    }

   private:
    base::WeakPtr<BiometricsManager> biometrics_manager_;
  };

  // Returned by StartEnrollSession to ensure that EnrollSession eventually
  // ends.
  using EnrollSession = Session<EnrollSessionEnder>;

  // Returned by StartAuthSession to ensure that AuthSession eventually
  // ends.
  using AuthSession = Session<AuthSessionEnder>;

  // Represents a record previously registered with this BiometricsManager in an
  // EnrollSession. These objects can be retrieved with GetRecords.
  class Record {
   public:
    virtual ~Record() {}
    virtual const std::string& GetId() const = 0;
    virtual const std::string& GetUserId() const = 0;
    virtual const std::string& GetLabel() const = 0;

    // Returns true on success.
    virtual bool SetLabel(std::string label) = 0;

    // Returns true on success.
    virtual bool Remove() = 0;
  };

  virtual ~BiometricsManager() {}
  virtual Type GetType() = 0;

  // Puts this BiometricsManager into EnrollSession mode, which can be ended by
  // letting the returned session fall out of scope. The user_id is arbitrary
  // and is given to AuthScanDone callbacks in the AuthSession object. The label
  // should be human readable and ideally from the user themselves. The label
  // can be read and modified from the Record objects. This will fail if ANY
  // other mode is active. Returns a false EnrollSession on failure.
  virtual EnrollSession StartEnrollSession(std::string user_id,
                                           std::string label) = 0;

  // Puts this BiometricsManager into AuthSession mode, which can be ended by
  // letting the returned session fall out of scope. This will fail if ANY other
  // mode is active. Returns a false AuthSession on failure.
  virtual AuthSession StartAuthSession() = 0;

  // Gets the records registered with this BiometricsManager. Some records will
  // naturally be unaccessible because they are currently in an encrypted state,
  // so those will silently be left out of the returned vector.
  virtual std::vector<std::unique_ptr<Record>> GetRecords() = 0;

  // Irreversibly destroys records registered with this BiometricsManager,
  // including currently encrypted ones. Returns true if successful.
  // TODO(mqg): right now it does not destroy the encrypted records, but that is
  // the goal for the future.
  virtual bool DestroyAllRecords() = 0;

  // Remove all decrypted records from memory. Still keep them in storage.
  virtual void RemoveRecordsFromMemory() = 0;

  // Read all the records for each of the users in the set. Return true if
  // successful.
  virtual bool ReadRecords(const std::unordered_set<std::string>& user_ids) = 0;

  // The callbacks should remain valid as long as this object is valid.

  // Invoked from EnrollSession mode whenever the user attempts a scan. The
  // first parameter tells whether the scan was successful. If it was
  // successful, the second parameter MAY be true to indicate that the record
  // was complete and is now over. However, it make take many successful scans
  // before this is true. When the record is complete, EnrollSession mode will
  // automatically be ended.
  using EnrollScanDoneCallback = base::Callback<void(ScanResult, bool done)>;
  virtual void SetEnrollScanDoneHandler(
      const EnrollScanDoneCallback& on_enroll_scan_done) = 0;

  // Invoked from AuthSession mode to indicate either a bad scan of any kind, or
  // a successful scan. In the case of successful scan, AttemptMatches is a map
  // of user id keys to a vector of record id values.
  using AttemptMatches =
      std::unordered_map<std::string, std::vector<std::string>>;
  using AuthScanDoneCallback = base::Callback<void(ScanResult, AttemptMatches)>;
  virtual void SetAuthScanDoneHandler(
      const AuthScanDoneCallback& on_auth_scan_done) = 0;

  // Invoked during any session to indicate that the session has ended with
  // failure. Any EnrollSession record that was underway is thrown away and
  // AuthSession will no longer be happening.
  using SessionFailedCallback = base::Callback<void()>;
  virtual void SetSessionFailedHandler(
      const SessionFailedCallback& on_session_failed) = 0;

 protected:
  virtual void EndEnrollSession() = 0;
  virtual void EndAuthSession() = 0;
};
}  // namespace biod

#endif  // BIOD_BIOMETRICS_MANAGER_H_
