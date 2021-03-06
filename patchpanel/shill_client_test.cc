// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "patchpanel/shill_client.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "patchpanel/fake_shill_client.h"

namespace patchpanel {

class ShillClientTest : public testing::Test {
 protected:
  void SetUp() override {
    helper_ = std::make_unique<FakeShillClientHelper>();
    client_ = helper_->FakeClient();
    client_->RegisterDefaultInterfaceChangedHandler(
        base::Bind(&ShillClientTest::DefaultInterfaceChangedHandler,
                   base::Unretained(this)));
    client_->RegisterDevicesChangedHandler(base::Bind(
        &ShillClientTest::DevicesChangedHandler, base::Unretained(this)));
    client_->RegisterIPConfigsChangedHandler(base::Bind(
        &ShillClientTest::IPConfigsChangedHandler, base::Unretained(this)));
    default_ifname_.clear();
    added_.clear();
    removed_.clear();
  }

  void DefaultInterfaceChangedHandler(const std::string& new_ifname,
                                      const std::string& prev_ifname) {
    default_ifname_ = new_ifname;
  }

  void DevicesChangedHandler(const std::set<std::string>& added,
                             const std::set<std::string>& removed) {
    added_ = added;
    removed_ = removed;
  }

  void IPConfigsChangedHandler(const std::string& device,
                               const ShillClient::IPConfig& ipconfig) {
    ipconfig_change_calls_.emplace_back(std::make_pair(device, ipconfig));
  }

 protected:
  std::string default_ifname_;
  std::set<std::string> added_;
  std::set<std::string> removed_;
  std::vector<std::pair<std::string, ShillClient::IPConfig>>
      ipconfig_change_calls_;
  std::unique_ptr<FakeShillClient> client_;
  std::unique_ptr<FakeShillClientHelper> helper_;
};

TEST_F(ShillClientTest, DevicesChangedHandlerCalledOnDevicesPropertyChange) {
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("eth0"),
                                           dbus::ObjectPath("wlan0")};
  auto value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  EXPECT_EQ(added_.size(), devices.size());
  EXPECT_EQ(removed_.size(), 0);
  for (const auto& d : devices) {
    EXPECT_NE(added_.find(d.value()), added_.end());
  }
  // Implies the default callback was run;
  EXPECT_NE(default_ifname_, "");
  EXPECT_NE(added_.find(default_ifname_), added_.end());

  devices.pop_back();
  devices.emplace_back(dbus::ObjectPath("eth1"));
  value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  EXPECT_EQ(added_.size(), 1);
  EXPECT_EQ(*added_.begin(), "eth1");
  EXPECT_EQ(removed_.size(), 1);
  EXPECT_EQ(*removed_.begin(), "wlan0");
}

TEST_F(ShillClientTest, VerifyDevicesPrefixStripped) {
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("/device/eth0")};
  auto value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  EXPECT_EQ(added_.size(), 1);
  EXPECT_EQ(*added_.begin(), "eth0");
  // Implies the default callback was run;
  EXPECT_EQ(default_ifname_, "eth0");
}

TEST_F(ShillClientTest,
       DefaultInterfaceChangedHandlerCalledOnNewDefaultInterface) {
  client_->SetFakeDefaultInterface("eth0");
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  EXPECT_EQ(default_ifname_, "eth0");

  client_->SetFakeDefaultInterface("wlan0");
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  EXPECT_EQ(default_ifname_, "wlan0");
}

TEST_F(ShillClientTest, DefaultInterfaceChangedHandlerNotCalledForSameDefault) {
  client_->SetFakeDefaultInterface("eth0");
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  EXPECT_EQ(default_ifname_, "eth0");

  default_ifname_.clear();
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  // Implies the callback was not run the second time.
  EXPECT_EQ(default_ifname_, "");
}

TEST_F(ShillClientTest, DefaultInterfaceFallbackUsingDevices) {
  // One network device appears.
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("wlan0")};
  auto value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  // That device is used as the fallback default interface.
  EXPECT_EQ(default_ifname_, "wlan0");

  // A second device appears.
  default_ifname_.clear();
  devices = {dbus::ObjectPath("eth0"), dbus::ObjectPath("wlan0")};
  value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  // The first device is still used as the fallback, the callback is not run.
  EXPECT_EQ(default_ifname_, "");

  // The second device becomes the default interface.
  client_->SetFakeDefaultInterface("eth0");
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  // The real default interface is preferred over the fallback interface.
  EXPECT_EQ(default_ifname_, "eth0");

  // The system loses the default interface.
  client_->SetFakeDefaultInterface("");
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  // The fallback interface is used instead.
  EXPECT_EQ(default_ifname_, "wlan0");

  // The first device disappears.
  devices = {dbus::ObjectPath("eth0")};
  value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  // The fallback interface is updated.
  EXPECT_EQ(default_ifname_, "eth0");

  // All devices have disappeared.
  devices = {};
  value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  // No device is used as the fallback default interface.
  EXPECT_EQ(default_ifname_, "");
}

TEST_F(ShillClientTest, ListenToDeviceChangeSignalOnNewDevices) {
  // Adds a device.
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("/wlan0")};
  auto value = brillo::Any(devices);
  EXPECT_CALL(*helper_->mock_proxy(),
              DoConnectToSignal(shill::kFlimflamDeviceInterface,
                                shill::kMonitorPropertyChanged, _, _))
      .Times(1);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);

  // Adds another device. DoConnectToSignal() called only for the new added one.
  devices = {dbus::ObjectPath("/wlan0"), dbus::ObjectPath("/eth0")};
  value = brillo::Any(devices);
  EXPECT_CALL(*helper_->mock_proxy(),
              DoConnectToSignal(shill::kFlimflamDeviceInterface,
                                shill::kMonitorPropertyChanged, _, _))
      .Times(1);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
}

TEST_F(ShillClientTest, TriggerOnIPConfigsChangeHandlerOnce) {
  // Adds a device.
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("/wlan0")};
  auto devices_value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, devices_value);
  client_->NotifyDevicePropertyChange("wlan0", shill::kIPConfigsProperty,
                                      brillo::Any());
  ASSERT_EQ(ipconfig_change_calls_.size(), 1u);
  EXPECT_EQ(ipconfig_change_calls_.back().first, "wlan0");

  // Removes the device and adds it again.
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, brillo::Any());
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, devices_value);
  client_->NotifyDevicePropertyChange("wlan0", shill::kIPConfigsProperty,
                                      brillo::Any());
  ASSERT_EQ(ipconfig_change_calls_.size(), 2u);
  EXPECT_EQ(ipconfig_change_calls_.back().first, "wlan0");
}

}  // namespace patchpanel
