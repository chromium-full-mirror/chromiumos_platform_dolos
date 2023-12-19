#!/usr/bin/env bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Create temporary directory
mkdir -p dolos_factory_tests

# Copy the files to the directory
cp conftest.py fw_uart.py __init__.py run_tests.py test_factory.py dolos_factory_tests

# Build the wheel file
python3 setup.py bdist_wheel

# Delete temporary directory
rm -rf dolos_factory_tests
