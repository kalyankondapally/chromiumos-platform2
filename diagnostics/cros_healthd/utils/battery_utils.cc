// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/battery_utils.h"

#include <cmath>

#include <base/strings/string_number_conversions.h>

#include "diagnostics/cros_healthd/utils/file_utils.h"

namespace diagnostics {

const char kBatteryDirectoryPath[] = "sys/class/power_supply/BAT0";
const char kBatteryChargeFullFileName[] = "charge_full";
const char kBatteryChargeFullDesignFileName[] = "charge_full_design";
const char kBatteryChargeNowFileName[] = "charge_now";
const char kBatteryCurrentNowFileName[] = "current_now";
const char kBatteryCycleCountFileName[] = "cycle_count";
const char kBatteryEnergyFullFileName[] = "energy_full";
const char kBatteryEnergyFullDesignFileName[] = "energy_full_design";
const char kBatteryManufacturerFileName[] = "manufacturer";
const char kBatteryPresentFileName[] = "present";
const char kBatteryStatusFileName[] = "status";
const char kBatteryVoltageNowFileName[] = "voltage_now";
const char kBatteryStatusChargingValue[] = "Charging";
const char kBatteryStatusDischargingValue[] = "Discharging";

base::Optional<double> CalculateBatteryChargePercent(
    const base::FilePath& root_dir) {
  base::FilePath battery_path = root_dir.Append(kBatteryDirectoryPath);

  uint32_t charge_now;
  if (!ReadInteger(battery_path, kBatteryChargeNowFileName, base::StringToUint,
                   &charge_now)) {
    return base::nullopt;
  }

  uint32_t charge_full;
  if (!ReadInteger(battery_path, kBatteryChargeFullFileName, base::StringToUint,
                   &charge_full)) {
    return base::nullopt;
  }

  return 100.0 *
         (static_cast<double>(charge_now) / static_cast<double>(charge_full));
}

}  // namespace diagnostics
