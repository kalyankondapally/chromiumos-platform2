// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "typecd/cable.h"

#include <gtest/gtest.h>

#include "typecd/test_constants.h"

namespace typecd {

class CableTest : public ::testing::Test {};

// Check the PD Identity cable speed logic for TBT3 compatibility mode entry
// for various cable PDO values.
// Since we don't have sysfs, we can just manually set the PD identity VDOs.
TEST_F(CableTest, TestTBT3PDIdentityCheck) {
  auto cable = std::make_unique<Cable>(base::FilePath(kFakePort0CableSysPath));

  // Apple Active TBT3 Pro Cable PD 3.0
  cable->SetPDRevision(kPDRevision30);
  cable->SetIdHeaderVDO(0x240005ac);
  cable->SetCertStatVDO(0x0);
  cable->SetProductVDO(0x72043002);
  cable->SetProductTypeVDO1(0x434858da);
  cable->SetProductTypeVDO2(0x5a5f0001);
  cable->SetProductTypeVDO3(0x0);
  EXPECT_TRUE(cable->TBT3PDIdentityCheck());

  // Apple Active TBT3 Pro Cable PD 2.0
  cable->SetPDRevision(kPDRevision20);
  cable->SetIdHeaderVDO(0x240005ac);
  cable->SetCertStatVDO(0x0);
  cable->SetProductVDO(0x72043002);
  cable->SetProductTypeVDO1(0x43085fda);
  cable->SetProductTypeVDO2(0x0);
  cable->SetProductTypeVDO3(0x0);
  EXPECT_TRUE(cable->TBT3PDIdentityCheck());

  // StarTech Passive Cable 40 Gbps PD 2.0
  cable->SetPDRevision(kPDRevision20);
  cable->SetIdHeaderVDO(0x1c0020c2);
  cable->SetCertStatVDO(0x000000b6);
  cable->SetProductVDO(0x00010310);
  cable->SetProductTypeVDO1(0x11082052);
  cable->SetProductTypeVDO2(0x0);
  cable->SetProductTypeVDO3(0x0);
  EXPECT_TRUE(cable->TBT3PDIdentityCheck());

  // Nekteck 100W USB 2.0 5A Cable PD 3.0
  cable->SetPDRevision(kPDRevision20);
  cable->SetIdHeaderVDO(0x18002e98);
  cable->SetCertStatVDO(0x00001533);
  cable->SetProductVDO(0x00010200);
  cable->SetProductTypeVDO1(0xc1082040);
  cable->SetProductTypeVDO2(0x0);
  cable->SetProductTypeVDO3(0x0);
  EXPECT_FALSE(cable->TBT3PDIdentityCheck());

  // Nekteck 100W USB 2.0 Cable PD 2.0
  cable->SetPDRevision(kPDRevision20);
  cable->SetIdHeaderVDO(0x18002e98);
  cable->SetCertStatVDO(0x00001533);
  cable->SetProductVDO(0x00010200);
  cable->SetProductTypeVDO1(0xc10827d0);
  cable->SetProductTypeVDO2(0x0);
  cable->SetProductTypeVDO3(0x0);
  EXPECT_FALSE(cable->TBT3PDIdentityCheck());
}

}  // namespace typecd
