// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_CONSTANTS_H_
#define UPDATE_ENGINE_PAYLOAD_CONSTANTS_H_

#include <stdint.h>

#include <limits>

namespace chromeos_update_engine {

extern const char kBspatchPath[];
extern const char kDeltaMagic[];

// A block number denoting a hole on a sparse file. Used on Extents to refer to
// section of blocks not present on disk on a sparse file.
const uint64_t kSparseHole = std::numeric_limits<uint64_t>::max();

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_CONSTANTS_H_
