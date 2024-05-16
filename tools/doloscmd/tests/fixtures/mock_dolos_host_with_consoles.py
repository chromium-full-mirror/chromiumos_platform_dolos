# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring
# pylint: disable=import-error
# pylint: disable=no-name-in-module

import logging

from doloscmd.tests.fixtures.mock_dolos_console import DolosStatus
import pytest


_logger = logging.getLogger("mock_dolos_host_with_console")


@pytest.fixture()
def mock_host_with_one_dolos(mock_dolos_host, mock_dolos_console):
    def generate_host(status=DolosStatus()):

        dolos_host = mock_dolos_host()
        dolos_console = mock_dolos_console(status=status)
        dolos_host.set_usb_devices(
            {"usb-FTDI_FT232R_USB_UART_B001I258-if00-port0": dolos_console}
        )

        return dolos_host

    return generate_host


@pytest.fixture()
def mock_host_with_multiple_dolos(mock_dolos_host, mock_dolos_console):
    def generate_host():

        dolos_host = mock_dolos_host()
        status = DolosStatus()
        status.serial_number = b"DOLOSV1-C-1520240001"
        dolos_console1 = mock_dolos_console(status=status)
        status.serial_number = b"DOLOSV1-C-1520240002"
        dolos_console2 = mock_dolos_console(status=status)
        status.serial_number = b"DOLOSV1-C-1520240003"
        dolos_console3 = mock_dolos_console(status=status)
        status.serial_number = b"DOLOSV1-C-1520240004"
        dolos_console4 = mock_dolos_console(status=status)
        status.serial_number = b"DOLOSV1-C-1520240005"
        dolos_console5 = mock_dolos_console(status=status)
        status.serial_number = b"DOLOSV1-C-1520240006"
        dolos_console6 = mock_dolos_console(status=status)
        status.serial_number = b"DOLOSV1-C-1520240007"
        dolos_console7 = mock_dolos_console(status=status)
        dolos_host.set_usb_devices(
            {
                "usb-FTDI_FT232R_USB_UART_B001I001-if00-port0": dolos_console1,
                "usb-FTDI_FT232R_USB_UART_B001I002-if00-port0": dolos_console2,
                "usb-FTDI_FT232R_USB_UART_B001I003-if00-port0": dolos_console3,
                "usb-FTDI_FT232R_USB_UART_B001I004-if00-port0": dolos_console4,
                "usb-FTDI_FT232R_USB_UART_B001I005-if00-port0": dolos_console5,
                "usb-FTDI_FT232R_USB_UART_B001I006-if00-port0": dolos_console6,
                "usb-FTDI_FT232R_USB_UART_B001I007-if00-port0": dolos_console7,
            }
        )
        return dolos_host

    return generate_host
