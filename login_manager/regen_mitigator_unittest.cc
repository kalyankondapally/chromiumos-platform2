// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/regen_mitigator.h"

#include <sys/types.h>
#include <unistd.h>

#include <string>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <gtest/gtest.h>

#include "chromeos/dbus/service_constants.h"
#include "login_manager/fake_browser_job.h"
#include "login_manager/fake_child_process.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_policy_key.h"
#include "login_manager/mock_system_utils.h"

using ::testing::Return;
using ::testing::StrEq;

namespace login_manager {

class RegenMitigatorTest : public ::testing::Test {
 public:
  RegenMitigatorTest() {}
  virtual ~RegenMitigatorTest() {}

 protected:
  MockSystemUtils utils_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegenMitigatorTest);
};

TEST_F(RegenMitigatorTest, Mitigate) {
  MockKeyGenerator gen;
  std::string fake_ownername("user");
  EXPECT_CALL(gen, Start(StrEq(fake_ownername), getuid()))
      .WillOnce(Return(true));
  RegenMitigator mitigator(&gen, true, getuid());
  EXPECT_TRUE(mitigator.Mitigate(fake_ownername));
}

}  // namespace login_manager
