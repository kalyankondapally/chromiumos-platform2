// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_FAKE_FAKE_ROUTINE_SERVICE_H_
#define DIAGNOSTICS_CROS_HEALTHD_FAKE_FAKE_ROUTINE_SERVICE_H_

#include <cstdint>
#include <string>

#include <base/macros.h>
#include <base/optional.h>

#include "mojo/cros_healthd.mojom.h"

namespace diagnostics {

// Fake implementation of the CrosHealthdDiagnosticsService interface.
class FakeRoutineService final
    : public chromeos::cros_healthd::mojom::CrosHealthdDiagnosticsService {
 public:
  using DiagnosticRoutineStatusEnum =
      chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum;
  using RunRoutineResponse = chromeos::cros_healthd::mojom::RunRoutineResponse;

  FakeRoutineService();
  ~FakeRoutineService() override;

  // chromeos::cros_healthd::mojom::CrosHealthdDiagnosticsService overrides:
  void GetAvailableRoutines(
      const GetAvailableRoutinesCallback& callback) override;
  void GetRoutineUpdate(
      int32_t id,
      chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      const GetRoutineUpdateCallback& callback) override;
  void RunUrandomRoutine(uint32_t length_seconds,
                         const RunUrandomRoutineCallback& callback) override;
  void RunBatteryCapacityRoutine(
      uint32_t low_mah,
      uint32_t high_mah,
      const RunBatteryCapacityRoutineCallback& callback) override;
  void RunBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed,
      const RunBatteryHealthRoutineCallback& callback) override;
  void RunSmartctlCheckRoutine(
      const RunSmartctlCheckRoutineCallback& callback) override;
  void RunAcPowerRoutine(
      chromeos::cros_healthd::mojom::AcPowerStatusEnum expected_status,
      const base::Optional<std::string>& expected_power_type,
      const RunAcPowerRoutineCallback& callback) override;
  void RunCpuCacheRoutine(uint32_t length_seconds,
                          const RunCpuCacheRoutineCallback& callback) override;
  void RunCpuStressRoutine(
      uint32_t length_seconds,
      const RunCpuStressRoutineCallback& callback) override;
  void RunFloatingPointAccuracyRoutine(
      uint32_t length_seconds,
      const RunFloatingPointAccuracyRoutineCallback& callback) override;
  void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      const RunNvmeWearLevelRoutineCallback& callback) override;
  void RunNvmeSelfTestRoutine(
      chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
      const RunNvmeSelfTestRoutineCallback& callback) override;
  void RunDiskReadRoutine(
      chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum type,
      uint32_t length_seconds,
      uint32_t file_size_mb,
      const RunDiskReadRoutineCallback& callback) override;
  void RunPrimeSearchRoutine(
      uint32_t length_seconds,
      uint64_t max_num,
      const RunPrimeSearchRoutineCallback& callback) override;
  void RunBatteryDischargeRoutine(
      uint32_t length_seconds,
      uint32_t maximum_discharge_percent_allowed,
      const RunBatteryDischargeRoutineCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRoutineService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_FAKE_FAKE_ROUTINE_SERVICE_H_
