// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/storage/emmc_device_adapter.h"

#include <string>

#include "diagnostics/common/file_utils.h"

namespace diagnostics {

namespace {

constexpr char kModelFile[] = "device/name";

}

EmmcDeviceAdapter::EmmcDeviceAdapter(const base::FilePath& dev_sys_path)
    : dev_sys_path_(dev_sys_path) {}

std::string EmmcDeviceAdapter::GetDeviceName() const {
  return dev_sys_path_.BaseName().value();
}

std::string EmmcDeviceAdapter::GetModel() const {
  std::string model;
  if (!ReadAndTrimString(dev_sys_path_, kModelFile, &model)) {
    return "";
  }
  return model;
}

}  // namespace diagnostics