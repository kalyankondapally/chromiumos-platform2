// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/source_delegate.h"

#include <base/values.h>

#include "settingsd/settings_keys.h"
#include "settingsd/settings_service.h"
#include "settingsd/source.h"

namespace settingsd {

bool DummySourceDelegate::ValidateVersionComponentBlob(
    const VersionComponentBlob& blob) const {
  return false;
}

bool DummySourceDelegate::ValidateSettingsBlob(const SettingsBlob& blob) const {
  return false;
}

std::unique_ptr<SourceDelegate> SourceDelegateFactory::operator()(
    const std::string& source_id,
    const SettingsService& settings) const {
  std::string type;
  const base::Value* type_value = settings.GetValue(
      MakeSourceKey(source_id).Extend({keys::sources::kType}));
  if (type_value && type_value->GetAsString(&type)) {
    auto entry = function_map_.find(type);
    if (entry != function_map_.end())
      return entry->second(source_id, settings);
  }

  // Type invalid or unknown, construct a dummy delegate.
  return std::unique_ptr<SourceDelegate>(new DummySourceDelegate());
}

void SourceDelegateFactory::RegisterFunction(
    const std::string& type,
    const SourceDelegateFactoryFunction& function) {
  function_map_[type] = function;
}

}  // namespace settingsd
