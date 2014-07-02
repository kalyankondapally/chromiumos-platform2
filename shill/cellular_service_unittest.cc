// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>

#include "shill/cellular_capability.h"
#include "shill/cellular_capability_cdma.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_cellular.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_out_of_credits_detector.h"
#include "shill/mock_profile.h"
#include "shill/mock_store.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"
#include "shill/service_property_change_test.h"

using std::string;
using testing::_;
using testing::InSequence;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace shill {

class CellularServiceTest : public testing::Test {
 public:
  CellularServiceTest()
      : modem_info_(NULL, &dispatcher_, NULL, NULL, NULL),
        device_(new MockCellular(&modem_info_,
                                 "usb0",
                                 kAddress,
                                 3,
                                 Cellular::kTypeCDMA,
                                 "",
                                 "",
                                 "",
                                 ProxyFactory::GetInstance())),
        service_(new CellularService(&modem_info_, device_)),
        adaptor_(NULL) {}

  virtual ~CellularServiceTest() {
    adaptor_ = NULL;
  }

  virtual void SetUp() {
    adaptor_ =
        dynamic_cast<ServiceMockAdaptor *>(service_->adaptor());
    out_of_credits_detector_ =
        new MockOutOfCreditsDetector(NULL, NULL, NULL, service_);
    // Passes ownership.
    service_->set_out_of_credits_detector(out_of_credits_detector_);
  }

  CellularCapabilityCDMA *GetCapabilityCDMA() {
    return dynamic_cast<CellularCapabilityCDMA *>(device_->capability_.get());
  }

 protected:
  static const char kAddress[];

  string GetFriendlyName() const { return service_->friendly_name(); }

  EventDispatcher dispatcher_;
  MockModemInfo modem_info_;
  scoped_refptr<MockCellular> device_;
  CellularServiceRefPtr service_;
  ServiceMockAdaptor *adaptor_;  // Owned by |service_|.
  MockOutOfCreditsDetector *out_of_credits_detector_;  // Owned by |service_|.
};

const char CellularServiceTest::kAddress[] = "000102030405";

TEST_F(CellularServiceTest, Constructor) {
  EXPECT_TRUE(service_->connectable());
}

TEST_F(CellularServiceTest, SetActivationState) {
  {
    InSequence call_sequence;
    EXPECT_CALL(*adaptor_, EmitStringChanged(
        kActivationStateProperty,
        kActivationStateNotActivated));
    EXPECT_CALL(*adaptor_, EmitBoolChanged(
        kConnectableProperty, false));
    EXPECT_CALL(*adaptor_, EmitStringChanged(
        kActivationStateProperty,
        kActivationStateActivating));
    EXPECT_CALL(*adaptor_, EmitBoolChanged(
        kConnectableProperty, true));
    EXPECT_CALL(*adaptor_, EmitStringChanged(
        kActivationStateProperty,
        kActivationStatePartiallyActivated));
    EXPECT_CALL(*adaptor_, EmitStringChanged(
        kActivationStateProperty,
        kActivationStateActivated));
    EXPECT_CALL(*adaptor_, EmitStringChanged(
        kActivationStateProperty,
        kActivationStateNotActivated));
    EXPECT_CALL(*adaptor_, EmitBoolChanged(
        kConnectableProperty, false));
  }
  EXPECT_CALL(*modem_info_.mock_manager(), HasService(_))
      .WillRepeatedly(Return(false));

  EXPECT_TRUE(service_->activation_state().empty());
  EXPECT_TRUE(service_->connectable());

  service_->SetActivationState(kActivationStateNotActivated);
  EXPECT_EQ(kActivationStateNotActivated, service_->activation_state());
  EXPECT_FALSE(service_->connectable());

  service_->SetActivationState(kActivationStateActivating);
  EXPECT_EQ(kActivationStateActivating, service_->activation_state());
  EXPECT_TRUE(service_->connectable());

  service_->SetActivationState(kActivationStatePartiallyActivated);
  EXPECT_EQ(kActivationStatePartiallyActivated, service_->activation_state());
  EXPECT_TRUE(service_->connectable());

  service_->SetActivationState(kActivationStateActivated);
  EXPECT_EQ(kActivationStateActivated, service_->activation_state());
  EXPECT_TRUE(service_->connectable());

  service_->SetActivationState(kActivationStateNotActivated);
  EXPECT_EQ(kActivationStateNotActivated, service_->activation_state());
  EXPECT_FALSE(service_->connectable());
}

TEST_F(CellularServiceTest, SetNetworkTechnology) {
  EXPECT_CALL(*adaptor_, EmitStringChanged(kNetworkTechnologyProperty,
                                           kNetworkTechnologyUmts));
  EXPECT_TRUE(service_->network_technology().empty());
  service_->SetNetworkTechnology(kNetworkTechnologyUmts);
  EXPECT_EQ(kNetworkTechnologyUmts, service_->network_technology());
  service_->SetNetworkTechnology(kNetworkTechnologyUmts);
}

TEST_F(CellularServiceTest, SetRoamingState) {
  EXPECT_CALL(*adaptor_, EmitStringChanged(kRoamingStateProperty,
                                           kRoamingStateHome));
  EXPECT_TRUE(service_->roaming_state().empty());
  service_->SetRoamingState(kRoamingStateHome);
  EXPECT_EQ(kRoamingStateHome, service_->roaming_state());
  service_->SetRoamingState(kRoamingStateHome);
}

TEST_F(CellularServiceTest, SetStorageIdentifier) {
  EXPECT_EQ(string(kTypeCellular) + "_" +
            kAddress + "_" + GetFriendlyName(),
            service_->GetStorageIdentifier());
  service_->SetStorageIdentifier("a b c");
  EXPECT_EQ("a_b_c", service_->GetStorageIdentifier());
}

TEST_F(CellularServiceTest, SetServingOperator) {
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(kServingOperatorProperty, _));
  static const char kCode[] = "123456";
  static const char kName[] = "Some Cellular Operator";
  Cellular::Operator oper;
  service_->SetServingOperator(oper);
  oper.SetCode(kCode);
  oper.SetName(kName);
  service_->SetServingOperator(oper);
  EXPECT_EQ(kCode, service_->serving_operator().GetCode());
  EXPECT_EQ(kName, service_->serving_operator().GetName());
  service_->SetServingOperator(oper);
}

TEST_F(CellularServiceTest, SetOLP) {
  const char kMethod[] = "GET";
  const char kURL[] = "payment.url";
  const char kPostData[] = "post_man";
  Stringmap olp;

  service_->SetOLP("", "", "");
  olp = service_->olp();  // Copy to simplify assertions below.
  EXPECT_EQ("", olp[kPaymentPortalURL]);
  EXPECT_EQ("", olp[kPaymentPortalMethod]);
  EXPECT_EQ("", olp[kPaymentPortalPostData]);

  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(kPaymentPortalProperty, _));
  service_->SetOLP(kURL, kMethod, kPostData);
  olp = service_->olp();  // Copy to simplify assertions below.
  EXPECT_EQ(kURL, olp[kPaymentPortalURL]);
  EXPECT_EQ(kMethod, olp[kPaymentPortalMethod]);
  EXPECT_EQ(kPostData, olp[kPaymentPortalPostData]);
}

TEST_F(CellularServiceTest, SetUsageURL) {
  static const char kUsageURL[] = "usage.url";
  EXPECT_CALL(*adaptor_, EmitStringChanged(kUsageURLProperty,
                                           kUsageURL));
  EXPECT_TRUE(service_->usage_url().empty());
  service_->SetUsageURL(kUsageURL);
  EXPECT_EQ(kUsageURL, service_->usage_url());
  service_->SetUsageURL(kUsageURL);
}

TEST_F(CellularServiceTest, SetApn) {
  static const char kApn[] = "TheAPN";
  static const char kUsername[] = "commander.data";
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      modem_info_.control_interface(), modem_info_.metrics(),
      modem_info_.manager()));
  service_->set_profile(profile);
  Error error;
  Stringmap testapn;
  testapn[kApnProperty] = kApn;
  testapn[kApnUsernameProperty] = kUsername;
  {
    InSequence seq;
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(kCellularLastGoodApnProperty,
                                     _));
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(kCellularApnProperty, _));
  }
  service_->SetApn(testapn, &error);
  EXPECT_TRUE(error.IsSuccess());
  Stringmap resultapn = service_->GetApn(&error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(2, resultapn.size());
  Stringmap::const_iterator it = resultapn.find(kApnProperty);
  EXPECT_TRUE(it != resultapn.end() && it->second == kApn);
  it = resultapn.find(kApnUsernameProperty);
  EXPECT_TRUE(it != resultapn.end() && it->second == kUsername);
  EXPECT_FALSE(service_->GetUserSpecifiedApn() == NULL);
}

TEST_F(CellularServiceTest, ClearApn) {
  static const char kApn[] = "TheAPN";
  static const char kUsername[] = "commander.data";
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      modem_info_.control_interface(), modem_info_.metrics(),
      modem_info_.manager()));
  service_->set_profile(profile);
  Error error;
  // Set up an APN to make sure that it later gets cleared.
  Stringmap testapn;
  testapn[kApnProperty] = kApn;
  testapn[kApnUsernameProperty] = kUsername;
  {
    InSequence seq;
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(kCellularLastGoodApnProperty,
                                     _));
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(kCellularApnProperty, _));
  }
  service_->SetApn(testapn, &error);
  Stringmap resultapn = service_->GetApn(&error);
  ASSERT_TRUE(error.IsSuccess());
  ASSERT_EQ(2, service_->GetApn(&error).size());

  Stringmap emptyapn;
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(kCellularLastGoodApnProperty,
                                   _)).Times(0);
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(kCellularApnProperty, _)).Times(1);
  service_->SetApn(emptyapn, &error);
  EXPECT_TRUE(error.IsSuccess());
  resultapn = service_->GetApn(&error);
  EXPECT_TRUE(resultapn.empty());
  EXPECT_TRUE(service_->GetUserSpecifiedApn() == NULL);
}

TEST_F(CellularServiceTest, LastGoodApn) {
  static const char kApn[] = "TheAPN";
  static const char kUsername[] = "commander.data";
  ProfileRefPtr profile(new NiceMock<MockProfile>(
      modem_info_.control_interface(), modem_info_.metrics(),
      modem_info_.manager()));
  service_->set_profile(profile);
  Stringmap testapn;
  testapn[kApnProperty] = kApn;
  testapn[kApnUsernameProperty] = kUsername;
  EXPECT_CALL(*adaptor_,
              EmitStringmapChanged(kCellularLastGoodApnProperty, _));
  service_->SetLastGoodApn(testapn);
  Stringmap *resultapn = service_->GetLastGoodApn();
  EXPECT_FALSE(resultapn == NULL);
  EXPECT_EQ(2, resultapn->size());
  Stringmap::const_iterator it = resultapn->find(kApnProperty);
  EXPECT_TRUE(it != resultapn->end() && it->second == kApn);
  it = resultapn->find(kApnUsernameProperty);
  EXPECT_TRUE(it != resultapn->end() && it->second == kUsername);
  // Now set the user-specified APN, and check that LastGoodApn got
  // cleared.
  Stringmap userapn;
  userapn[kApnProperty] = kApn;
  userapn[kApnUsernameProperty] = kUsername;
  {
    InSequence seq;
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(kCellularLastGoodApnProperty,
                                     _));
    EXPECT_CALL(*adaptor_,
                EmitStringmapChanged(kCellularApnProperty, _));
  }
  Error error;
  service_->SetApn(userapn, &error);
  EXPECT_TRUE(service_->GetLastGoodApn() == NULL);
}

TEST_F(CellularServiceTest, IsAutoConnectable) {
  const char *reason = NULL;

  ON_CALL(*out_of_credits_detector_, IsDetecting())
      .WillByDefault(Return(false));

  // Auto-connect should be suppressed if the device is not running.
  device_->running_ = false;
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(CellularService::kAutoConnDeviceDisabled, reason);

  device_->running_ = true;

  // If we're waiting on a disconnect before an activation, don't auto-connect.
  GetCapabilityCDMA()->activation_starting_ = true;
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));

  // If we're waiting on an activation, also don't auto-connect.
  GetCapabilityCDMA()->activation_starting_ = false;
  GetCapabilityCDMA()->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));

  GetCapabilityCDMA()->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;

  // Auto-connect should be suppressed if the we're undergoing an
  // out-of-credits detection.
  EXPECT_CALL(*out_of_credits_detector_, IsDetecting())
      .WillOnce(Return(true));
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(CellularService::kAutoConnOutOfCreditsDetectionInProgress,
               reason);
  Mock::VerifyAndClearExpectations(out_of_credits_detector_);

  // Auto-connect should be suppressed if we're out of credits.
  EXPECT_CALL(*out_of_credits_detector_, IsDetecting())
      .WillOnce(Return(false));
  EXPECT_CALL(*out_of_credits_detector_, out_of_credits())
      .WillOnce(Return(true));
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(CellularService::kAutoConnOutOfCredits, reason);
  Mock::VerifyAndClearExpectations(out_of_credits_detector_);

  EXPECT_CALL(*out_of_credits_detector_, out_of_credits())
      .WillRepeatedly(Return(false));

  // But other activation states are fine.
  GetCapabilityCDMA()->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));
  GetCapabilityCDMA()->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));
  GetCapabilityCDMA()->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED;
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A PPP authentication failure means the Service is not auto-connectable.
  service_->SetFailure(Service::kFailurePPPAuth);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));

  // Reset failure state, to make the Service auto-connectable again.
  service_->SetState(Service::kStateIdle);
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // The following test cases are copied from ServiceTest.IsAutoConnectable

  service_->SetConnectable(true);
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // We should not auto-connect to a Service that a user has
  // deliberately disconnected.
  Error error;
  service_->UserInitiatedDisconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnExplicitDisconnect, reason);

  // But if the Service is reloaded, it is eligible for auto-connect
  // again.
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(service_->GetStorageIdentifier()))
      .WillOnce(Return(true));
  EXPECT_TRUE(service_->Load(&storage));
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A non-user initiated Disconnect doesn't change anything.
  service_->Disconnect(&error);
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  // A resume also re-enables auto-connect.
  service_->UserInitiatedDisconnect(&error);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  service_->OnAfterResume();
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));

  service_->SetState(Service::kStateConnected);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnConnected, reason);

  service_->SetState(Service::kStateAssociating);
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ(Service::kAutoConnConnecting, reason);
}

TEST_F(CellularServiceTest, LoadResetsPPPAuthFailure) {
  NiceMock<MockStore> storage;
  EXPECT_CALL(storage, ContainsGroup(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(storage, GetString(_, _, _)).WillRepeatedly(Return(true));

  const string kDefaultUser;
  const string kDefaultPass;
  const string kNewUser("new-username");
  const string kNewPass("new-password");
  for (const auto change_username : { false, true }) {
    for (const auto change_password : { false, true }) {
      service_->ppp_username_ = kDefaultUser;
      service_->ppp_password_ = kDefaultPass;
      service_->SetFailure(Service::kFailurePPPAuth);
      EXPECT_TRUE(service_->IsFailed());
      EXPECT_EQ(Service::kFailurePPPAuth, service_->failure());
      if (change_username) {
        EXPECT_CALL(storage,
                    GetString(_, CellularService::kStoragePPPUsername, _))
            .WillOnce(DoAll(SetArgumentPointee<2>(kNewUser), Return(true)))
            .RetiresOnSaturation();
      }
      if (change_password) {
        EXPECT_CALL(storage,
                    GetString(_, CellularService::kStoragePPPPassword, _))
            .WillOnce(DoAll(SetArgumentPointee<2>(kNewPass), Return(true)))
            .RetiresOnSaturation();
      }
      EXPECT_TRUE(service_->Load(&storage));
      if (change_username || change_password) {
        EXPECT_NE(Service::kFailurePPPAuth, service_->failure());
      } else {
        EXPECT_EQ(Service::kFailurePPPAuth, service_->failure());
      }
    }
  }
}

// Some of these tests duplicate signals tested above. However, it's
// convenient to have all the property change notifications documented
// (and tested) in one place.
TEST_F(CellularServiceTest, PropertyChanges) {
  TestCommonPropertyChanges(service_, adaptor_);
  TestAutoConnectPropertyChange(service_, adaptor_);

  bool activate_over_non_cellular =
      service_->activate_over_non_cellular_network();
  EXPECT_CALL(*adaptor_,
              EmitBoolChanged(kActivateOverNonCellularNetworkProperty, _));
  service_->SetActivateOverNonCellularNetwork(!activate_over_non_cellular);
  Mock::VerifyAndClearExpectations(adaptor_);

  EXPECT_NE(kActivationStateNotActivated, service_->activation_state());
  EXPECT_CALL(*adaptor_, EmitStringChanged(kActivationStateProperty, _));
  service_->SetActivationState(kActivationStateNotActivated);
  Mock::VerifyAndClearExpectations(adaptor_);

  string network_technology = service_->network_technology();
  EXPECT_CALL(*adaptor_, EmitStringChanged(kNetworkTechnologyProperty, _));
  service_->SetNetworkTechnology(network_technology + "and some new stuff");
  Mock::VerifyAndClearExpectations(adaptor_);

  bool out_of_credits = true;
  EXPECT_CALL(*adaptor_,
              EmitBoolChanged(kOutOfCreditsProperty, out_of_credits));
  service_->SignalOutOfCreditsChanged(out_of_credits);
  Mock::VerifyAndClearExpectations(adaptor_);

  string roaming_state = service_->roaming_state();
  EXPECT_CALL(*adaptor_, EmitStringChanged(kRoamingStateProperty, _));
  service_->SetRoamingState(roaming_state + "and some new stuff");
  Mock::VerifyAndClearExpectations(adaptor_);
}

// Custom property setters should return false, and make no changes, if
// the new value is the same as the old value.
TEST_F(CellularServiceTest, CustomSetterNoopChange) {
  // Test that we didn't break any setters provided by the base class.
  TestCustomSetterNoopChange(service_, modem_info_.mock_manager());

  // Test the new setter we added.
  // First set up our environment...
  static const char kApn[] = "TheAPN";
  static const char kUsername[] = "commander.data";
  Error error;
  Stringmap testapn;
  ProfileRefPtr profile(new NiceMock<MockProfile>(nullptr, nullptr, nullptr));
  service_->set_profile(profile);
  testapn[kApnProperty] = kApn;
  testapn[kApnUsernameProperty] = kUsername;
  // ... then set to a known value ...
  EXPECT_TRUE(service_->SetApn(testapn, &error));
  EXPECT_TRUE(error.IsSuccess());
  // ... then set to same value.
  EXPECT_FALSE(service_->SetApn(testapn, &error));
  EXPECT_TRUE(error.IsSuccess());
}

}  // namespace shill
