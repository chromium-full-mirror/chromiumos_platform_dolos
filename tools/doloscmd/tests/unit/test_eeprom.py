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
from doloscmd.console_lib import DolosConsoleException
from doloscmd.console_lib import EEPROM_SIZE
from doloscmd.tests.fixtures.mock_dolos_console import DolosEEPROM
import pytest


class TestEEPROM:
    def test_eeprom_read(self, mock_dolos_host, mock_dolos_console):
        eeprom = DolosEEPROM()
        dolos_host = mock_dolos_host()
        dolos_console = mock_dolos_console(eeprom=eeprom)
        dolos_host.set_usb_devices(
            {"usb-FTDI_FT232R_USB_UART_B001I258-if00-port0": dolos_console}
        )
        console = DolosConsole.find_dolos_serial()
        assert isinstance(console, DolosConsole)
        data = console.eeprom_read()
        assert isinstance(data, bytes)
        assert data == bytes(eeprom.data)

    def test_eeprom_write(self, mock_dolos_host, mock_dolos_console):
        eeprom = DolosEEPROM()
        dolos_host = mock_dolos_host()
        dolos_console = mock_dolos_console(eeprom=eeprom)
        dolos_host.set_usb_devices(
            {"usb-FTDI_FT232R_USB_UART_B001I258-if00-port0": dolos_console}
        )
        console = DolosConsole.find_dolos_serial()
        assert isinstance(console, DolosConsole)
        data = console.eeprom_read()
        inverted = bytes([x ^ 0xFF for x in data])
        console.eeprom_write(inverted)
        data = console.eeprom_read()
        assert data == inverted
        assert isinstance(data, bytes)
        assert bytes(eeprom.data) == inverted
