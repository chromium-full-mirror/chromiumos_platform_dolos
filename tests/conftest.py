"""Fixtures and extra options for factory test suite"""

# Lint as: python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pytest

from . import fw_uart


@pytest.fixture(scope="module")
def fw_cmd(request):
    uart_path = request.config.getoption("uart")
    baudrate = request.config.getoption("baudrate")
    return fw_uart.FwUart(uart_path, baudrate)


def pytest_addoption(parser):
    parser.addoption("--uart", action="store", default="/dev/ttyUSB0")
    parser.addoption("--baudrate", action="store", default="115200")
    parser.addoption("--zephyr", action="store_true", help="Run Zephyr-specific tests")
