// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TYPECD_PORT_H_
#define TYPECD_PORT_H_

#include <map>
#include <memory>
#include <utility>

#include <gtest/gtest_prod.h>

#include "typecd/partner.h"

namespace typecd {

// This class is used to represent a Type C Port. It can be used to access PD
// state associated with the port, and will also contain handles to the object
// representing a peripheral (i.e "Partner") if one is connected to the port.
class Port {
 public:
  static std::unique_ptr<Port> CreatePort(const base::FilePath& syspath);
  Port(const base::FilePath& syspath, int port_num);

  void AddPartner(const base::FilePath& path);
  void RemovePartner();

 private:
  friend class PortTest;
  FRIEND_TEST(PortTest, TestBasicAdd);

  // Sysfs path used to access partner PD information.
  base::FilePath syspath_;
  // Port number as described by the Type C connector class framework.
  int port_num_;
  std::unique_ptr<Partner> partner_;
};

}  // namespace typecd

#endif  // TYPECD_PORT_H_