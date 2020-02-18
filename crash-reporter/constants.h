// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CONSTANTS_H_
#define CRASH_REPORTER_CONSTANTS_H_

namespace constants {

constexpr char kCrashName[] = "crash";
constexpr char kCrashGroupName[] = "crash-access";
#if !USE_KVM_GUEST
constexpr char kCrashUserGroupName[] = "crash-user-access";
#endif
}

#endif  // CRASH_REPORTER_CONSTANTS_H_
