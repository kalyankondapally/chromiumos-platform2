// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_SERVER_MOCK_SERVICE_PUBLISHER_H_
#define P2P_SERVER_MOCK_SERVICE_PUBLISHER_H_

#include "p2p/server/fake_service_publisher.h"

#include <map>
#include <string>

#include <gmock/gmock.h>

namespace p2p {

namespace server {

class MockServicePublisher : public ServicePublisher {
 public:
  MockServicePublisher() {
    // Delegate all calls to the fake instance
    ON_CALL(*this, AddFile(testing::_, testing::_))
        .WillByDefault(testing::Invoke(&fake_, &FakeServicePublisher::AddFile));
    ON_CALL(*this, RemoveFile(testing::_))
        .WillByDefault(
            testing::Invoke(&fake_, &FakeServicePublisher::RemoveFile));
    ON_CALL(*this, UpdateFileSize(testing::_, testing::_))
        .WillByDefault(
            testing::Invoke(&fake_, &FakeServicePublisher::UpdateFileSize));
    ON_CALL(*this, SetNumConnections(testing::_))
        .WillByDefault(
            testing::Invoke(&fake_, &FakeServicePublisher::SetNumConnections));
    ON_CALL(*this, files())
        .WillByDefault(testing::Invoke(&fake_, &FakeServicePublisher::files));
  }

  MOCK_METHOD(void, AddFile, (const std::string&, size_t), (override));
  MOCK_METHOD(void, RemoveFile, (const std::string&), (override));
  MOCK_METHOD(void, UpdateFileSize, (const std::string&, size_t), (override));
  MOCK_METHOD(void, SetNumConnections, (int), (override));
  MOCK_METHOD((std::map<std::string, size_t>), files, (), (override));

  FakeServicePublisher& fake() { return fake_; }

 private:
  FakeServicePublisher fake_;

  DISALLOW_COPY_AND_ASSIGN(MockServicePublisher);
};

}  // namespace server

}  // namespace p2p

#endif  // P2P_SERVER_MOCK_SERVICE_PUBLISHER_H_
