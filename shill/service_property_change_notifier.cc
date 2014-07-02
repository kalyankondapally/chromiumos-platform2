// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service_property_change_notifier.h"

#include <string>

#include <base/bind.h>

#include "shill/adaptor_interfaces.h"
#include "shill/property_observer.h"

using base::Bind;
using std::string;

namespace shill {

ServicePropertyChangeNotifier::ServicePropertyChangeNotifier(
    ServiceAdaptorInterface *adaptor) : rpc_adaptor_(adaptor) {}

ServicePropertyChangeNotifier::~ServicePropertyChangeNotifier() {}

void ServicePropertyChangeNotifier::AddBoolPropertyObserver(
    const string &name, BoolAccessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<bool>(
          accessor,
          Bind(&ServicePropertyChangeNotifier::BoolPropertyUpdater,
               base::Unretained(this),
               name)));
}

void ServicePropertyChangeNotifier::AddUint8PropertyObserver(
    const string &name, Uint8Accessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<uint8>(
          accessor,
          Bind(&ServicePropertyChangeNotifier::Uint8PropertyUpdater,
               base::Unretained(this),
               name)));
}

void ServicePropertyChangeNotifier::AddUint16PropertyObserver(
    const string &name, Uint16Accessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<uint16>(
          accessor,
          Bind(&ServicePropertyChangeNotifier::Uint16PropertyUpdater,
               base::Unretained(this),
               name)));
}

void ServicePropertyChangeNotifier::AddUint16sPropertyObserver(
    const string &name, Uint16sAccessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<Uint16s>(
          accessor,
          Bind(&ServiceAdaptorInterface::EmitUint16sChanged,
               base::Unretained(rpc_adaptor_),
               name)));
}

void ServicePropertyChangeNotifier::AddUintPropertyObserver(
    const string &name, Uint32Accessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<uint32>(
          accessor,
          Bind(&ServicePropertyChangeNotifier::Uint32PropertyUpdater,
               base::Unretained(this),
               name)));
}

void ServicePropertyChangeNotifier::AddIntPropertyObserver(
    const string &name, Int32Accessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<int32>(
          accessor,
          Bind(&ServicePropertyChangeNotifier::Int32PropertyUpdater,
               base::Unretained(this),
               name)));
}

void ServicePropertyChangeNotifier::AddRpcIdentifierPropertyObserver(
    const string &name, RpcIdentifierAccessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<string>(
          accessor,
          Bind(&ServiceAdaptorInterface::EmitRpcIdentifierChanged,
               base::Unretained(rpc_adaptor_),
               name)));
}

void ServicePropertyChangeNotifier::AddStringPropertyObserver(
    const string &name, StringAccessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<string>(
          accessor,
          Bind(&ServiceAdaptorInterface::EmitStringChanged,
               base::Unretained(rpc_adaptor_),
               name)));
}

void ServicePropertyChangeNotifier::AddStringmapPropertyObserver(
    const string &name, StringmapAccessor accessor) {
  property_observers_.emplace_back(
      new PropertyObserver<Stringmap>(
          accessor,
          Bind(&ServiceAdaptorInterface::EmitStringmapChanged,
               base::Unretained(rpc_adaptor_),
               name)));
}

void ServicePropertyChangeNotifier::UpdatePropertyObservers() {
  for (const auto &observer : property_observers_) {
    observer->Update();
  }
}

void ServicePropertyChangeNotifier::BoolPropertyUpdater(const string &name,
                                                        const bool &value) {
  rpc_adaptor_->EmitBoolChanged(name, value);
}

void ServicePropertyChangeNotifier::Uint8PropertyUpdater(const string &name,
                                                         const uint8 &value) {
  rpc_adaptor_->EmitUint8Changed(name, value);
}

void ServicePropertyChangeNotifier::Uint16PropertyUpdater(const string &name,
                                                          const uint16 &value) {
  rpc_adaptor_->EmitUint16Changed(name, value);
}

void ServicePropertyChangeNotifier::Uint32PropertyUpdater(const string &name,
                                                          const uint32 &value) {
  rpc_adaptor_->EmitUintChanged(name, value);
}

void ServicePropertyChangeNotifier::Int32PropertyUpdater(const string &name,
                                                         const int32 &value) {
  rpc_adaptor_->EmitIntChanged(name, value);
}

}  // namespace shill
