// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/rollback_helper.h"

#include <set>
#include <vector>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/logging.h>

#include "oobe_config/oobe_config.h"
#include "oobe_config/rollback_constants.h"

namespace oobe_config {

const int kDefaultPwnameLength = 1024;

bool PrepareSave(const base::FilePath& root_path,
                 bool ignore_permissions_for_testing) {
  // Make sure we have an empty folder where only we can write, otherwise exit.
  base::FilePath save_path = PrefixAbsolutePath(root_path, kSaveTempPath);
  if (!base::DeleteFile(save_path, true)) {
    PLOG(ERROR) << "Couldn't delete directory " << save_path.value();
    return false;
  }
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(save_path, &error)) {
    PLOG(ERROR) << "Couldn't create directory " << save_path.value()
                << ", error: " << error;
    return false;
  }

  base::FilePath rollback_data_path =
      PrefixAbsolutePath(root_path, kUnencryptedStatefulRollbackDataPath);

  if (!ignore_permissions_for_testing) {
    uid_t oobe_config_save_uid;
    gid_t oobe_config_save_gid;
    uid_t root_uid;
    gid_t root_gid;

    if (!GetUidGid(kOobeConfigSaveUsername, &oobe_config_save_uid,
                   &oobe_config_save_gid)) {
      PLOG(ERROR) << "Couldn't get uid and gid of oobe_config_save.";
      return false;
    }
    if (!GetUidGid(kRootUsername, &root_uid, &root_gid)) {
      PLOG(ERROR) << "Couldn't get uid and gid of root.";
      return false;
    }
    // chown oobe_config_save:oobe_config_save
    if (lchown(save_path.value().c_str(), oobe_config_save_uid,
               oobe_config_save_gid) != 0) {
      PLOG(ERROR) << "Couldn't chown " << save_path.value();
      return false;
    }
    // chmod 700
    if (chmod(save_path.value().c_str(), 0700) != 0) {
      PLOG(ERROR) << "Couldn't chmod " << save_path.value();
      return false;
    }
    if (!base::VerifyPathControlledByUser(save_path, save_path,
                                          oobe_config_save_uid,
                                          {oobe_config_save_gid})) {
      LOG(ERROR) << "VerifyPathControlledByUser failed for "
                 << save_path.value();
      return false;
    }

    // Preparing rollback_data file.

    // The directory should be root-writeable only.
    if (!base::VerifyPathControlledByUser(
            PrefixAbsolutePath(root_path, kStatefulPartition),
            rollback_data_path.DirName(), root_uid, {root_gid})) {
      LOG(ERROR) << "VerifyPathControlledByUser failed for "
                 << rollback_data_path.DirName().value();
      return false;
    }
    // Create or wipe the file.
    if (base::WriteFile(rollback_data_path, {}, 0) != 0) {
      PLOG(ERROR) << "Couldn't write " << rollback_data_path.value();
      return false;
    }
    // chown oobe_config_save:oobe_config_save
    if (lchown(rollback_data_path.value().c_str(), oobe_config_save_uid,
               oobe_config_save_gid) != 0) {
      PLOG(ERROR) << "Couldn't chown " << rollback_data_path.value();
      return false;
    }
    // chmod 644
    if (chmod(rollback_data_path.value().c_str(), 0644) != 0) {
      PLOG(ERROR) << "Couldn't chmod " << rollback_data_path.value();
      return false;
    }
    // The file should be only writable by kOobeConfigSaveUid.
    if (!base::VerifyPathControlledByUser(
            rollback_data_path, rollback_data_path, oobe_config_save_uid,
            {oobe_config_save_gid})) {
      LOG(ERROR) << "VerifyPathControlledByUser failed for "
                 << rollback_data_path.value();
      return false;
    }
  }

  TryFileCopy(PrefixAbsolutePath(root_path, kInstallAttributesPath),
              save_path.Append(kInstallAttributesFileName));
  TryFileCopy(PrefixAbsolutePath(root_path, kOwnerKeyFilePath),
              save_path.Append(kOwnerKeyFileName));
  TryFileCopy(PrefixAbsolutePath(root_path, kShillDefaultProfilePath),
              save_path.Append(kShillDefaultProfileFileName));

  base::FileEnumerator policy_file_enumerator(
      PrefixAbsolutePath(root_path, kPolicyFileDirectory), false,
      base::FileEnumerator::FILES, kPolicyFileNamePattern);
  for (auto file = policy_file_enumerator.Next(); !file.empty();
       file = policy_file_enumerator.Next()) {
    TryFileCopy(file, save_path.Append(file.BaseName()));
  }

  return true;
}

bool FinishRestore(const base::FilePath& root_path,
                   bool ignore_permissions_for_testing) {
  OobeConfig oobe_config;
  if (!root_path.empty()) {
    oobe_config.set_prefix_path_for_testing(root_path);
  }

  if (!oobe_config.CheckSecondStage()) {
    LOG(ERROR) << "Finish restore is not in stage 2.";
    return false;
  }

  LOG(INFO) << "Starting rollback restore stage 2.";
  base::FilePath restore_path = PrefixAbsolutePath(root_path, kRestoreTempPath);
  // Restore install attributes. /home/.shadow should already exist at OOBE
  // time. Owner should be root:root, with permissions 644.
  if (!CopyFileAndSetPermissions(
          restore_path.Append(kInstallAttributesFileName),
          PrefixAbsolutePath(root_path, kInstallAttributesPath), kRootUsername,
          0644, ignore_permissions_for_testing)) {
    LOG(ERROR) << "Couldn't restore install attributes.";
  }

  // Restore owner.key. /var/lib/whitelist/ should already exist at OOBE
  // time. Owner should be root:root, with permissions 604.
  if (!CopyFileAndSetPermissions(
          restore_path.Append(kOwnerKeyFileName),
          PrefixAbsolutePath(root_path, kOwnerKeyFilePath), kRootUsername, 0604,
          ignore_permissions_for_testing)) {
    LOG(ERROR) << "Couldn't restore owner.key.";
  }

  // Restore shill default profile. /var/cache/shill/ should already exist at
  // OOBE time. The file is restored with owner root:root, permissions 600,
  // shill will take care of setting these properly in shill-pre-start.sh.
  if (!CopyFileAndSetPermissions(
          restore_path.Append(kShillDefaultProfileFileName),
          PrefixAbsolutePath(root_path, kShillDefaultProfilePath),
          kRootUsername, 0600, ignore_permissions_for_testing)) {
    LOG(ERROR) << "Couldn't restore shill default profile.";
  }

  // Restore policy files. /var/lib/whitelist/ should already exist at OOBE
  // time. Owner should be root:root, with permissions 604.
  base::FileEnumerator policy_file_enumerator(
      restore_path, false, base::FileEnumerator::FILES, kPolicyFileNamePattern);
  for (auto file = policy_file_enumerator.Next(); !file.empty();
       file = policy_file_enumerator.Next()) {
    if (!CopyFileAndSetPermissions(
            file,
            PrefixAbsolutePath(root_path, kPolicyFileDirectory)
                .Append(file.BaseName()),
            kRootUsername, 0604, ignore_permissions_for_testing)) {
      LOG(ERROR) << "Couldn't restore policy.";
    }
  }

  // Delete all files from the directory except the ones needed
  // for stage 3.
  LOG(INFO) << "Cleaning up rollback restore stage 1 and 2 files.";
  std::set<std::string> excluded_files;
  excluded_files.emplace(
      PrefixAbsolutePath(root_path, kFirstStageCompletedFile).value());
  excluded_files.emplace(
      PrefixAbsolutePath(root_path, kEncryptedStatefulRollbackDataPath)
          .value());

  base::FileEnumerator folder_enumerator(
      restore_path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (auto file = folder_enumerator.Next(); !file.empty();
       file = folder_enumerator.Next()) {
    if (excluded_files.count(file.value()) == 0) {
      if (!base::DeleteFile(file, true)) {
        PLOG(ERROR) << "Couldn't delete " << file.value();
      } else {
        LOG(INFO) << "Deleted rollback data file: " << file.value();
      }
    } else {
      LOG(INFO) << "Preserving rollback data file: " << file.value();
    }
  }

  // Delete the original preserved data.
  base::FilePath rollback_data_file =
      PrefixAbsolutePath(root_path, kUnencryptedStatefulRollbackDataPath);
  if (!base::DeleteFile(rollback_data_file, true)) {
    PLOG(ERROR) << "Couldn't delete " << rollback_data_file.value();
  } else {
    LOG(INFO) << "Deleted encrypted rollback data.";
  }

  // Indicate that the second stage completed.
  oobe_config.WriteFile(kSecondStageCompletedFile, "");
  LOG(INFO) << "Rollback restore stage 2 completed.";

  return true;
}

base::FilePath PrefixAbsolutePath(const base::FilePath& prefix,
                                  const base::FilePath& file_path) {
  if (prefix.empty())
    return file_path;
  DCHECK(!file_path.empty());
  DCHECK_EQ('/', file_path.value()[0]);
  return prefix.Append(file_path.value().substr(1));
}

void TryFileCopy(const base::FilePath& source,
                 const base::FilePath& destination) {
  if (!base::CopyFile(source, destination)) {
    PLOG(WARNING) << "Couldn't copy file " << source.value() << " to "
                  << destination.value();
  } else {
    LOG(INFO) << "Copied " << source.value() << " to " << destination.value();
  }
}

bool CopyFileAndSetPermissions(const base::FilePath& source,
                               const base::FilePath& destination,
                               const std::string& owner_username,
                               mode_t permissions,
                               bool ignore_permissions_for_testing) {
  if (!base::PathExists(source.DirName())) {
    LOG(ERROR) << "Parent path doesn't exist: " << source.DirName().value();
    return false;
  }
  TryFileCopy(source, destination);
  if (!ignore_permissions_for_testing) {
    uid_t owner_user;
    gid_t owner_group;
    if (!GetUidGid(owner_username, &owner_user, &owner_group)) {
      PLOG(ERROR) << "Couldn't get uid and gid of user " << owner_username;
      return false;
    }
    if (lchown(destination.value().c_str(), owner_user, owner_group) != 0) {
      PLOG(ERROR) << "Couldn't chown " << destination.value();
      return false;
    }
    if (chmod(destination.value().c_str(), permissions) != 0) {
      PLOG(ERROR) << "Couldn't chmod " << destination.value();
      return false;
    }
  }
  return true;
}

bool GetUidGid(const std::string& user, uid_t* uid, gid_t* gid) {
  // Load the passwd entry.
  int user_name_length = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (user_name_length == -1) {
    user_name_length = kDefaultPwnameLength;
  }
  if (user_name_length < 0) {
    return false;
  }
  passwd user_info{}, *user_infop;
  std::vector<char> user_name_buf(static_cast<size_t>(user_name_length));
  if (getpwnam_r(user.c_str(), &user_info, user_name_buf.data(),
                 static_cast<size_t>(user_name_length), &user_infop)) {
    return false;
  }
  *uid = user_info.pw_uid;
  *gid = user_info.pw_gid;
  return true;
}

}  // namespace oobe_config
