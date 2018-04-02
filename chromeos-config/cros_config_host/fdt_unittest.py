#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The unit test suite for the fdt.py library"""

from __future__ import print_function

import os
import unittest

from . import fdt, fdt_util

DTS_FILE = '../libcros_config/test.dts'
ANOTHER_FIRMWARE_NAMES = ['bcs-overlay', 'ec-image', 'main-image',
                          'main-rw-image', 'key-id']


class FdtLibTest(unittest.TestCase):
  """The unit test suite for the fdt.py library"""
  def setUp(self):
    path = os.path.join(os.path.dirname(__file__), DTS_FILE)
    temp_file = None
    try:
      (fname, temp_file) = fdt_util.EnsureCompiled(path)
      with open(fname) as fdt_file:
        self.test_fdt = fdt.Fdt(fdt_file)
    finally:
      if temp_file is not None:
        os.remove(temp_file.name)
    self.test_fdt.Scan()

  def testFdtScan(self):
    self.assertIsNotNone(self.test_fdt.GetRoot())

  def testGetModels(self):
    models_node = self.test_fdt.GetNode('/chromeos/models')
    models = [m.name for m in models_node.subnodes.values()]
    self.assertSequenceEqual(models, ['some', 'another', 'whitelabel'])

  def testPropertyOrder(self):
    firmware = self.test_fdt.GetNode('/chromeos/models/another/firmware')

    self.assertSequenceEqual(firmware.props.keys(), ANOTHER_FIRMWARE_NAMES)

  def testGetStringProperty(self):
    firmware = self.test_fdt.GetNode('/chromeos/models/another/firmware')
    bcs_overlay = firmware.props['bcs-overlay'].value
    self.assertEqual(bcs_overlay, 'overlay-another-private')
    firmware = self.test_fdt.GetNode('/chromeos/models/another')
    value = firmware.props['wallpaper'].value
    self.assertEqual(value, 'default')

  def testLookupPhandle(self):
    firmware = self.test_fdt.GetNode('/chromeos/models/some/firmware')
    shared = self.test_fdt.GetNode('/chromeos/family/firmware/some')
    self.assertEqual(shared, firmware.props['shares'].LookupPhandle())

    # Phandles are sequentially allocated integers > 0, so 0 is invalid
    self.assertEqual(None, self.test_fdt.LookupPhandle(0))

  def testGetStringListProperty(self):
    firmware = self.test_fdt.GetNode('/chromeos/models/another')
    str_list = firmware.props['string-list'].value
    self.assertEqual(str_list, ['default', 'more'])

  def testGetBoolProperty(self):
    firmware = self.test_fdt.GetNode('/chromeos/models/another')
    present = firmware.props['bool-prop'].value
    self.assertEqual(present, True)


if __name__ == '__main__':
  unittest.main()
