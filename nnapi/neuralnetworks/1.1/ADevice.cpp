// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the boilerplate implementation of the IAllocator HAL interface,
// generated by the hidl-gen tool and then modified for use on Chrome OS.
// Modifications include:
// - Removal of non boiler plate client and server related code.
// - Reformatting to meet the Chrome OS coding standards.
//
// Originally generated with the command:
// $ hidl-gen -o output -L c++-adapter -r android.hardware:hardware/interfaces \
//   android.hardware.neuralnetworks@1.1

#include <android/hardware/neuralnetworks/1.1/ADevice.h>
#include <android/hardware/neuralnetworks/1.0/ADevice.h>
#include <android/hardware/neuralnetworks/1.0/AExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.0/APreparedModel.h>
#include <android/hardware/neuralnetworks/1.0/APreparedModelCallback.h>
#include <android/hardware/neuralnetworks/1.1/IDevice.h>
#include <hidladapter/HidlBinderAdapter.h>

namespace android {
namespace hardware {
namespace neuralnetworks {
namespace V1_1 {

ADevice::ADevice(
    const ::android::sp<::android::hardware::neuralnetworks::V1_1::IDevice>&
        impl)
    : mImpl(impl) {
}  // Methods from ::android::hardware::neuralnetworks::V1_0::IDevice follow.
::android::hardware::Return<void> ADevice::getCapabilities(
    getCapabilities_cb _hidl_cb) {
  getCapabilities_cb _hidl_cb_wrapped =
      [&](::android::hardware::neuralnetworks::V1_0::ErrorStatus status,
          const ::android::hardware::neuralnetworks::V1_0::Capabilities&
              capabilities) { return _hidl_cb(status, capabilities); };
  auto _hidl_out = mImpl->getCapabilities(_hidl_cb_wrapped);
  return _hidl_out;
}
::android::hardware::Return<void> ADevice::getSupportedOperations(
    const ::android::hardware::neuralnetworks::V1_0::Model& model,
    getSupportedOperations_cb _hidl_cb) {
  getSupportedOperations_cb _hidl_cb_wrapped =
      [&](::android::hardware::neuralnetworks::V1_0::ErrorStatus status,
          const ::android::hardware::hidl_vec<bool>& supportedOperations) {
        return _hidl_cb(status, supportedOperations);
      };
  auto _hidl_out = mImpl->getSupportedOperations(model, _hidl_cb_wrapped);
  return _hidl_out;
}
::android::hardware::Return<
    ::android::hardware::neuralnetworks::V1_0::ErrorStatus>
ADevice::prepareModel(
    const ::android::hardware::neuralnetworks::V1_0::Model& model,
    const ::android::sp<
        ::android::hardware::neuralnetworks::V1_0::IPreparedModelCallback>&
        callback) {
  auto _hidl_out = mImpl->prepareModel(
      model,
      static_cast<::android::sp<
          ::android::hardware::neuralnetworks::V1_0::IPreparedModelCallback>>(
          ::android::hardware::neuralnetworks::V1_0::IPreparedModelCallback::
              castFrom(::android::hardware::details::adaptWithDefault(
                  static_cast<
                      ::android::sp<::android::hardware::neuralnetworks::V1_0::
                                        IPreparedModelCallback>>(callback),
                  [&] {
                    return new ::android::hardware::neuralnetworks::V1_0::
                        APreparedModelCallback(callback);
                  }))));
  if (!_hidl_out.isOkUnchecked()) {
    return _hidl_out;
  }
  return _hidl_out;
}
::android::hardware::Return<
    ::android::hardware::neuralnetworks::V1_0::DeviceStatus>
ADevice::getStatus() {
  auto _hidl_out = mImpl->getStatus();
  if (!_hidl_out.isOkUnchecked()) {
    return _hidl_out;
  }
  return _hidl_out;
}

// Methods from ::android::hardware::neuralnetworks::V1_1::IDevice follow.
::android::hardware::Return<void> ADevice::getCapabilities_1_1(
    getCapabilities_1_1_cb _hidl_cb) {
  getCapabilities_1_1_cb _hidl_cb_wrapped =
      [&](::android::hardware::neuralnetworks::V1_0::ErrorStatus status,
          const ::android::hardware::neuralnetworks::V1_1::Capabilities&
              capabilities) { return _hidl_cb(status, capabilities); };
  auto _hidl_out = mImpl->getCapabilities_1_1(_hidl_cb_wrapped);
  return _hidl_out;
}
::android::hardware::Return<void> ADevice::getSupportedOperations_1_1(
    const ::android::hardware::neuralnetworks::V1_1::Model& model,
    getSupportedOperations_1_1_cb _hidl_cb) {
  getSupportedOperations_1_1_cb _hidl_cb_wrapped =
      [&](::android::hardware::neuralnetworks::V1_0::ErrorStatus status,
          const ::android::hardware::hidl_vec<bool>& supportedOperations) {
        return _hidl_cb(status, supportedOperations);
      };
  auto _hidl_out = mImpl->getSupportedOperations_1_1(model, _hidl_cb_wrapped);
  return _hidl_out;
}
::android::hardware::Return<
    ::android::hardware::neuralnetworks::V1_0::ErrorStatus>
ADevice::prepareModel_1_1(
    const ::android::hardware::neuralnetworks::V1_1::Model& model,
    ::android::hardware::neuralnetworks::V1_1::ExecutionPreference preference,
    const ::android::sp<
        ::android::hardware::neuralnetworks::V1_0::IPreparedModelCallback>&
        callback) {
  auto _hidl_out = mImpl->prepareModel_1_1(
      model, preference,
      static_cast<::android::sp<
          ::android::hardware::neuralnetworks::V1_0::IPreparedModelCallback>>(
          ::android::hardware::neuralnetworks::V1_0::IPreparedModelCallback::
              castFrom(::android::hardware::details::adaptWithDefault(
                  static_cast<
                      ::android::sp<::android::hardware::neuralnetworks::V1_0::
                                        IPreparedModelCallback>>(callback),
                  [&] {
                    return new ::android::hardware::neuralnetworks::V1_0::
                        APreparedModelCallback(callback);
                  }))));
  if (!_hidl_out.isOkUnchecked()) {
    return _hidl_out;
  }
  return _hidl_out;
}

// Methods from ::android::hidl::base::V1_0::IBase follow.

}  // namespace V1_1
}  // namespace neuralnetworks
}  // namespace hardware
}  // namespace android