// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DNS_CLIENT_H_
#define SHILL_MOCK_DNS_CLIENT_H_

#include <string>

#include "shill/dns_client.h"

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockDnsClient : public DnsClient {
 public:
  MockDnsClient();
  ~MockDnsClient() override;

  MOCK_METHOD(bool, Start, (const std::string&, Error*), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(bool, IsActive, (), (const, override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDnsClient);
};

}  // namespace shill

#endif  // SHILL_MOCK_DNS_CLIENT_H_
