// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TYPECD_ALT_MODE_H_
#define TYPECD_ALT_MODE_H_

#include <memory>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>

namespace typecd {

// Class used to represent the altmodes supported by a partner or cable.
class AltMode {
 public:
  // Factory function to create an alternate mode object, given a filepath.
  // Returns:
  //  An alternate mode object on success, and nullptr otherwise.
  static std::unique_ptr<AltMode> CreateAltMode(const base::FilePath& syspath);

  explicit AltMode(const base::FilePath& syspath) : syspath_(syspath) {}

  uint16_t GetSVID() const { return svid_; }
  uint32_t GetVDO() const { return vdo_; }

 private:
  // Get the latest values of the alt mode properties from sysfs.
  //
  // Returns:
  //  True if all properties were successfully update, and false otherwise.
  //
  // The properties of the AltMode are only updated if *all* properties were
  // read successfully.
  bool UpdateValuesFromSysfs();

  uint16_t svid_;
  uint32_t vdo_;
  // The index of the VDO for this alt mode in the Discover Mode response.
  int mode_index_;
  base::FilePath syspath_;

  DISALLOW_COPY_AND_ASSIGN(AltMode);
};

}  // namespace typecd

#endif  //  TYPECD_ALT_MODE_H_
