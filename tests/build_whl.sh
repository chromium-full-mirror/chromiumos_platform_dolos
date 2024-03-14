#!/usr/bin/env bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Create temporary directory
curr_dir=$(pwd)
mkdir -p dolos_factory_tests

#Building dolos firmware file
if [ -n "$1" ]; then
    cd "${curr_dir}/../firmware" || exit
    ./build.sh  "$1"
    cd "${curr_dir}" || exit
    cp "${curr_dir}/../firmware/Debug/dolos-"*.txt "dolos_factory_tests/dolos.txt"
else
    cp "${curr_dir}/../firmware/Debug/dolos-"*.txt "dolos_factory_tests/dolos.txt"
fi

#building fw-updater
cd "${curr_dir}/../fw-updater" || exit
cargo build
cd "${curr_dir}" || exit
cp "${curr_dir}/../fw-updater/target/debug/fw-updater" dolos_factory_tests

# Copy the files to the directory
cp conftest.py fw_uart.py __init__.py run_tests.py test_factory.py dolos_factory_tests

# Build the wheel file
python3 setup.py bdist_wheel

# Delete temporary directory
rm -rf dolos_factory_tests
