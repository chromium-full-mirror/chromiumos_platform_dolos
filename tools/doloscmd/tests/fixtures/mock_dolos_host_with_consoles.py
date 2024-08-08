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
        devices = {}
        for i in range(8):
            uart = f"usb-FTDI_FT232R_USB_UART_B001I00{i}-if00-port0"
            serial_number = f"DOLOSV1-C-152024000{i}"
            status = DolosStatus()
            status.serial_number = serial_number
            devices[uart] = mock_dolos_console(status=status)

        dolos_host.set_usb_devices(devices)
        return dolos_host

    return generate_host
