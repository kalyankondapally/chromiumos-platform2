// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/archive_manager.h"

#include <brillo/process/process_reaper.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/metrics.h"
#include "cros-disks/platform.h"

namespace cros_disks {
namespace {

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::IsEmpty;
using testing::Return;
using testing::SetArgPointee;
using testing::UnorderedElementsAre;

const char kMountRootDirectory[] = "/my_mount_point";

// Mock Platform implementation for testing.
class MockPlatform : public Platform {
 public:
  MOCK_METHOD(bool,
              GetUserAndGroupId,
              (const std::string&, uid_t*, gid_t*),
              (const, override));
  MOCK_METHOD(bool,
              GetGroupId,
              (const std::string&, gid_t*),
              (const, override));
};

class ArchiveManagerUnderTest : public ArchiveManager {
 public:
  using ArchiveManager::ArchiveManager;
  ~ArchiveManagerUnderTest() override { UnmountAll(); }

  MOCK_METHOD(bool, CanMount, (const std::string&), (const, override));
  MOCK_METHOD(std::unique_ptr<MountPoint>,
              DoMount,
              (const std::string&,
               const std::string&,
               const std::vector<std::string>&,
               const base::FilePath&,
               MountOptions*,
               MountErrorType*),
              (override));
};

}  // namespace

class ArchiveManagerTest : public testing::Test {
 protected:
  Metrics metrics_;
  MockPlatform platform_;
  brillo::ProcessReaper reaper_;
  const ArchiveManagerUnderTest manager_{kMountRootDirectory, &platform_,
                                         &metrics_, &reaper_};
};

TEST_F(ArchiveManagerTest, IsInAllowedFolder) {
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(""));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/dev/sda1"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/MyFiles/foo"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/MyFiles/x/"
      "foo"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/MyFiles/"
      "Downloads/foo"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/MyFiles/"
      "Downloads/x/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/x/foo"));
  EXPECT_FALSE(
      ArchiveManager::IsInAllowedFolder("/home/chronos/user/MyFiles/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/homex/chronos/u-0123456789abcdef0123456789abcdef01234567/MyFiles/"
      "Downloads/x/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronosx/u-0123456789abcdef0123456789abcdef01234567/MyFiles/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/0123456789abcdef0123456789abcdef01234567/MyFiles/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567x/MyFiles/foo"));
  EXPECT_FALSE(
      ArchiveManager::IsInAllowedFolder("/home/chronos/user/Downloads/bar"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/MyFiles/"
      "Downloads/bar"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/media/removable"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/media/removable/"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/media/archive"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/media/archive/"));
  EXPECT_FALSE(
      ArchiveManager::IsInAllowedFolder("/home/chronos/user/Downloads"));
  EXPECT_FALSE(
      ArchiveManager::IsInAllowedFolder("/home/chronos/user/Downloads/"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/Downloads"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/Downloads/"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/home/chronos/bar"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/home/chronos/user/bar"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/bar"));
  EXPECT_FALSE(
      ArchiveManager::IsInAllowedFolder("/home/chronos/Downloads/bar"));
  EXPECT_FALSE(
      ArchiveManager::IsInAllowedFolder("/home/chronos/foo/Downloads/bar"));
  EXPECT_FALSE(
      ArchiveManager::IsInAllowedFolder("/home/chronos/u-/Downloads/bar"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-0123456789abcdef0123456789abcdef0123456/Downloads/bar"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder(
      "/home/chronos/u-xyz3456789abcdef0123456789abcdef01234567/Downloads/"
      "bar"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder("/media/archive/y/foo"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder("/media/fuse/y/foo"));
  EXPECT_TRUE(ArchiveManager::IsInAllowedFolder("/media/removable/y/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/media/x/y/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("/media/x/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("x/media/fuse/y/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("media/fuse/y/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("file:///media/fuse/y/foo"));
  EXPECT_FALSE(ArchiveManager::IsInAllowedFolder("ssh:///media/fuse/y/foo"));
}

TEST_F(ArchiveManagerTest, GetMountSourceType) {
  EXPECT_EQ(manager_.GetMountSourceType(), MOUNT_SOURCE_ARCHIVE);
}

TEST_F(ArchiveManagerTest, SuggestMountPath) {
  EXPECT_EQ(manager_.SuggestMountPath("/home/chronos/Downloads/My Doc.rar"),
            std::string(kMountRootDirectory) + "/My Doc.rar");
  EXPECT_EQ(manager_.SuggestMountPath("/media/archive/Test.rar/My Doc.zip"),
            std::string(kMountRootDirectory) + "/My Doc.zip");
}

TEST_F(ArchiveManagerTest, GetSupplementaryGroups) {
  const gid_t gid = 478785;
  EXPECT_CALL(platform_, GetGroupId("android-everybody", _))
      .WillOnce(DoAll(SetArgPointee<1>(gid), Return(true)));
  EXPECT_THAT(manager_.GetSupplementaryGroups(), UnorderedElementsAre(gid));
}

TEST_F(ArchiveManagerTest, GetSupplementaryGroupsCannotGetGroupId) {
  EXPECT_CALL(platform_, GetGroupId("android-everybody", _))
      .WillOnce(Return(false));
  EXPECT_THAT(manager_.GetSupplementaryGroups(), IsEmpty());
}

TEST_F(ArchiveManagerTest, GetMountOptions) {
  const uid_t uid = 687123;
  const gid_t gid = 932648;
  EXPECT_CALL(platform_, GetUserAndGroupId("chronos", _, nullptr))
      .WillOnce(DoAll(SetArgPointee<1>(uid), Return(true)));
  EXPECT_CALL(platform_, GetGroupId("chronos-access", _))
      .WillOnce(DoAll(SetArgPointee<1>(gid), Return(true)));

  MountOptions options;
  EXPECT_EQ(manager_.GetMountOptions(&options), MOUNT_ERROR_NONE);
  EXPECT_EQ(
      options.ToString(),
      "ro,uid=687123,gid=932648,nodev,noexec,nosuid,umask=0222,nosymfollow");
}

TEST_F(ArchiveManagerTest, GetMountOptionsCannotGetGroupId) {
  const uid_t uid = 687123;
  EXPECT_CALL(platform_, GetUserAndGroupId("chronos", _, nullptr))
      .WillOnce(DoAll(SetArgPointee<1>(uid), Return(true)));
  EXPECT_CALL(platform_, GetGroupId("chronos-access", _))
      .WillOnce(Return(false));

  MountOptions options;
  EXPECT_EQ(manager_.GetMountOptions(&options), MOUNT_ERROR_INTERNAL);
}

TEST_F(ArchiveManagerTest, GetMountOptionsCannotGetUserId) {
  EXPECT_CALL(platform_, GetUserAndGroupId("chronos", _, nullptr))
      .WillOnce(Return(false));

  MountOptions options;
  EXPECT_EQ(manager_.GetMountOptions(&options), MOUNT_ERROR_INTERNAL);
}

}  // namespace cros_disks
