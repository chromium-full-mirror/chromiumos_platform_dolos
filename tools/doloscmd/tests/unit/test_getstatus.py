# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=redefined-outer-name
# pylint: disable=unused-argument
# pylint: disable=import-error
# pylint: disable=missing-module-docstring
# pylint: disable=missing-class-docstring
# pylint: disable=missing-function-docstring

from doloscmd.console_lib import DolosConsole
from doloscmd.error import DolosConsoleError
from doloscmd.tests.fixtures.mock_dolos_console import DolosStatus
import pytest


class TestGetStatus:
    def test_standard_serial_no(self, mock_dolos_host, mock_host_with_one_dolos):
        status = DolosStatus()
        status.serial_number = "DOLOSV1-C-2403140001"
        host = mock_host_with_one_dolos(status=status)
        result = DolosConsole(list(host.usb_devices.keys())[0]).get_status()
        assert result["Serial number"] == "DOLOSV1-C-2403140001"

    def test_broken_serial_no(self, mock_dolos_host, mock_host_with_one_dolos):
        status = DolosStatus()
        status.serial_number = "dolos-V1-2425-002E"
        host = mock_host_with_one_dolos(status=status)
        console = DolosConsole(list(host.usb_devices.keys())[0])
        result = console.get_status()
        assert result["Serial number"] == "DOLOSV1-C-2520240046"
        assert console.serial == "DOLOSV1-C-2520240046"

    def test_bad_crc(self, mock_dolos_host, mock_host_with_one_dolos):
        status = DolosStatus()
        status.serial_number = "DOLOSV1-C-2403140001"
        status.eeprom = "CRC Failure."
        host = mock_host_with_one_dolos(status=status)
        result = DolosConsole(list(host.usb_devices.keys())[0]).get_status()
        assert result["Serial number"] == "DOLOSV1-C-2403140001"

    def test_bad_crc(self, mock_dolos_host, mock_host_with_one_dolos):
        status = DolosStatus()
        status.serial_number = "No cable - can't read serial."
        status.eeprom = "EEPROM Read Error."
        host = mock_host_with_one_dolos(status=status)
        result = DolosConsole(list(host.usb_devices.keys())[0]).get_status()
        assert result["Serial number"] == "No cable - can't read serial."
