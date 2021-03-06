// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "typecd/partner.h"

#include <string>

#include <base/strings/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "typecd/alt_mode.h"
#include "typecd/test_constants.h"
#include "typecd/test_utils.h"

namespace {

const uint32_t kPartnerPDProductVDO = 0xdeadbeef;
const uint32_t kPartnerPDProductVDO2 = 0xabcdabcd;
const uint32_t kPartnerPDCertStatVDO = 0xbeefdead;
const uint32_t kPartnerPDIdHeaderVDO = 0x12341234;

}  // namespace

namespace typecd {

class PartnerTest : public ::testing::Test {};

// Check that calls of AddAltMode() done explicitly function correctly. Also
// check that trying to Add the same alt mode twice fails.
TEST_F(PartnerTest, TestAltModeManualAddition) {
  Partner p((base::FilePath(kFakePort0PartnerSysPath)));

  // Set up fake sysfs paths.
  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &temp_dir));

  std::string mode0_dirname =
      base::StringPrintf("port%d-partner.%d", 0, kDPAltModeIndex);
  auto mode0_path = temp_dir.Append(mode0_dirname);
  ASSERT_TRUE(CreateFakeAltMode(mode0_path, kDPSVID, kDPVDO, kDPVDOIndex));

  EXPECT_TRUE(p.AddAltMode(mode0_path));

  std::string mode1_dirname =
      base::StringPrintf("port%d-partner.%d", 0, kTBTAltModeIndex);
  auto mode1_path = temp_dir.Append(mode1_dirname);
  ASSERT_TRUE(CreateFakeAltMode(mode1_path, kTBTSVID, kTBTVDO, kTBTVDOIndex));

  // Add extra white spaces to ensure malformed strings can be parsed. We can do
  // this by overwriting whatever the pre-existing SVID syspath file is.
  auto mode1_svid = base::StringPrintf("%x    ", kTBTSVID);
  ASSERT_TRUE(base::WriteFile(mode1_path.Append("svid"), mode1_svid.c_str(),
                              mode1_svid.length()));

  EXPECT_TRUE(p.AddAltMode(mode1_path));
  // Trying to add an existing alt mode again should fail.
  EXPECT_FALSE(p.AddAltMode(mode1_path));
}

// Verify that partner PD identity VDOs get scanned and stored correctly.
// Also check that once PD identity VDOs are scanned, subsequent changes to PD
// identity aren't considered.
TEST_F(PartnerTest, TestPDIdentityScan) {
  // Set up fake sysfs paths.
  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &temp_dir));

  auto partner_path = temp_dir.Append(std::string("port0-partner"));
  ASSERT_TRUE(base::CreateDirectory(partner_path));

  auto identity_path = partner_path.Append(std::string("identity"));
  ASSERT_TRUE(base::CreateDirectory(identity_path));

  // First fill the identity with 0 values.
  auto cert_stat_vdo = base::StringPrintf("0x0");
  ASSERT_TRUE(base::WriteFile(identity_path.Append("cert_stat"),
                              cert_stat_vdo.c_str(), cert_stat_vdo.length()));
  auto id_header_vdo = base::StringPrintf("0x0");
  ASSERT_TRUE(base::WriteFile(identity_path.Append("id_header"),
                              id_header_vdo.c_str(), id_header_vdo.length()));
  auto product_vdo = base::StringPrintf("0x0");
  ASSERT_TRUE(base::WriteFile(identity_path.Append("product"),
                              product_vdo.c_str(), product_vdo.length()));

  Partner p(partner_path);

  // Update the VDOs with some values
  cert_stat_vdo = base::StringPrintf("%#x", kPartnerPDCertStatVDO);
  ASSERT_TRUE(base::WriteFile(identity_path.Append("cert_stat"),
                              cert_stat_vdo.c_str(), cert_stat_vdo.length()));
  id_header_vdo = base::StringPrintf("%#x", kPartnerPDIdHeaderVDO);
  ASSERT_TRUE(base::WriteFile(identity_path.Append("id_header"),
                              id_header_vdo.c_str(), id_header_vdo.length()));
  product_vdo = base::StringPrintf("%#x", kPartnerPDProductVDO);
  ASSERT_TRUE(base::WriteFile(identity_path.Append("product"),
                              product_vdo.c_str(), product_vdo.length()));

  // Since we don't have a UdevMonitor, trigger the PD VDO update manually.
  p.UpdatePDIdentityVDOs();
  EXPECT_EQ(kPartnerPDCertStatVDO, p.GetCertStateVDO());
  EXPECT_EQ(kPartnerPDIdHeaderVDO, p.GetIdHeaderVDO());
  EXPECT_EQ(kPartnerPDProductVDO, p.GetProductVDO());

  // Fake an update to the Product VDO, then ensure it doesn't get accepted.
  product_vdo = base::StringPrintf("%#x", kPartnerPDProductVDO2);
  ASSERT_TRUE(base::WriteFile(identity_path.Append("product"),
                              product_vdo.c_str(), product_vdo.length()));
  p.UpdatePDIdentityVDOs();

  EXPECT_NE(kPartnerPDProductVDO2, p.GetProductVDO());
}

}  // namespace typecd
