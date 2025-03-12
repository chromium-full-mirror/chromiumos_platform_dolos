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
import pytest


class TestFindDolos:
    def test_find_one(self, mock_dolos_host):
        dolos_host = mock_dolos_host()
        dolos_host.set_usb_devices(
            {
                "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0": None,
            }
        )
        result = DolosConsole._get_all_ftdi_uart_names()
        assert len(result) == 1
        assert "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0" in str(result)

    def test_find_two(self, mock_dolos_host):
        dolos_host = mock_dolos_host()
        dolos_host.set_usb_devices(
            {
                "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0": None,
                "usb-FTDI_FT232R_USB_UART_B001I999-if00-port0": None,
            }
        )
        devices = DolosConsole._get_all_ftdi_uart_names()
        assert 2 == len(devices)
        assert "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0" in str(devices)
        assert "usb-FTDI_FT232R_USB_UART_B001I999-if00-port0" in str(devices)

    def test_find_one_nomatch(self, mock_dolos_host):
        dolos_host = mock_dolos_host()
        dolos_host.set_usb_devices(
            {
                "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0": None,
                "no_match": None,
            }
        )
        devices = DolosConsole._get_all_ftdi_uart_names()
        assert 1 == len(devices)
        assert "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0" in str(devices)

    def test_find_none(self, mock_dolos_host):
        dolos_host = mock_dolos_host()
        dolos_host.clear_usb_devices()
        assert DolosConsole._get_all_ftdi_uart_names() == []


class TestFindDolosWithoutArgs:
    def test_find_without_serial_none(self, mock_dolos_host):
        dolos_host = mock_dolos_host()
        dolos_host.clear_usb_devices()
        with pytest.raises(DolosConsoleError):
            console = DolosConsole.find_dolos_serial()

    def test_find_without_serial_one(self, mock_host_with_one_dolos):
        dolos_host = mock_host_with_one_dolos()
        console = DolosConsole.find_dolos_serial()
        assert isinstance(console, DolosConsole)

    def test_find_without_serial_two(self, mock_host_with_multiple_dolos):
        dolos_host = mock_host_with_multiple_dolos()
        with pytest.raises(DolosConsoleError):
            console = DolosConsole.find_dolos_serial()
