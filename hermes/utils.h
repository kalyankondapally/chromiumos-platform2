// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_UTILS_H_
#define HERMES_UTILS_H_

#include <utility>

namespace hermes {

// Create a std::array from a set of values without manually specifying the
// size of the array. Note that unlike the make_array likely to make its way
// into C++20, this function always requires the user to specify ElementType.
// This is done so that users are not surprised by the element type of resulting
// arrays when std::common_type is used.
template <typename ElementType, typename... T>
constexpr auto make_array(T&&... values) {
  return std::array<ElementType, sizeof...(T)>{
    static_cast<ElementType>(std::forward<T>(values))...};
}

}  // namespace hermes

#endif  // HERMES_UTILS_H_
