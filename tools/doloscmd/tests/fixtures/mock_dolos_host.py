# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=unused-argument
# pylint: disable=import-error
# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring
# pylint: disable=import-error
# pylint: disable=no-name-in-module


import logging
import os
from pathlib import PosixPath
import re

from doloscmd.console_lib import SERIAL_PATH
import pytest
import serial


_logger = logging.getLogger("mock_dolos_host")


@pytest.fixture(scope="function")
def mock_dolos_host(class_mocker):
    def generate_dolos_host():
        """Mock generator function.   This allows multiple tests to be run in
        parallel as it generates a new mock for each test vs sharing the same
        mock between tests.

        Returns:
            MockDolosHost:
        """

        class MockDolosHost:
            def __init__(self):
                self.usb_devices = {}
                class_mocker.patch("serial.Serial", side_effect=self.mock_serial_open)
                class_mocker.patch(
                    "pathlib.PosixPath.glob",
                    side_effect=self.mock_glob,
                )

            def set_usb_devices(self, usb_devices):
                self.usb_devices = usb_devices

            def clear_usb_devices(self):
                self.usb_devices = {}

            def raise_on_open(self):
                class_mocker.patch(
                    "serial.Serial", side_effect=serial.SerialException()
                )

            def mock_glob(self, glob_str):
                matched_usb_devices = [
                    device for device in self.usb_devices if re.match(glob_str, device)
                ]
                return [
                    PosixPath(f"{SERIAL_PATH}{device}")
                    for device in matched_usb_devices
                ]

            # pylint
            def mock_serial_open(
                self, port, baudrate, exclusive, timeout, write_timeout
            ):
                if not os.path.basename(port) in self.usb_devices:
                    raise serial.SerialException(f"Device not found {port}.")
                return self.usb_devices[os.path.basename(port)]

        return MockDolosHost()

    return generate_dolos_host
