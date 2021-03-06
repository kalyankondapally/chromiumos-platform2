// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_MOCK_OPENVPN_MANAGEMENT_SERVER_H_
#define SHILL_VPN_MOCK_OPENVPN_MANAGEMENT_SERVER_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/vpn/openvpn_management_server.h"

namespace shill {

class MockOpenVPNManagementServer : public OpenVPNManagementServer {
 public:
  MockOpenVPNManagementServer();
  ~MockOpenVPNManagementServer() override;

  MOCK_METHOD(bool,
              Start,
              (Sockets*, std::vector<std::vector<std::string>>*),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, ReleaseHold, (), (override));
  MOCK_METHOD(void, Hold, (), (override));
  MOCK_METHOD(void, Restart, (), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOpenVPNManagementServer);
};

}  // namespace shill

#endif  // SHILL_VPN_MOCK_OPENVPN_MANAGEMENT_SERVER_H_
