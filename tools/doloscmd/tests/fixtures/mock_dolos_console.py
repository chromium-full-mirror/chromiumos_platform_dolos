# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring

import contextlib
import logging

from doloscmd.console_lib import CONSOLE_CLEAR_CHR
from doloscmd.console_lib import CONSOLE_ENTER
import pytest


_logger = logging.getLogger("mock_dolos_host")


class DolosStatus:  # pylint: disable=too-many-instance-attributes, too-few-public-methods

    template = (
        b"status\r\n"
        b"Dolos status:\r\n"
        b"    Uptime             : %(uptime)b\r\n"
        b"    Charger            : %(charger)b\r\n"
        b"    E-Fuse Power       : %(efuse)b\r\n"
        b"    System present     : %(system_present)b\r\n"
        b"    BMS current state  : %(bms_state)b\r\n"
        b"    BMS next state     : Next States can be BMS_STATE_POWER_GOOD_WAIT"
        b"or BMS_STATE_POWER_OUTPUT_OFF\r\n"
        b"                         Transition to BMS_STATE_POWER_GOOD_WAIT happens"
        b" when the system and charger are connected and the input voltage is out of "
        b"efuse range.\r\n"
        b"                         Transition to BMS_STATE_POWER_OUTPUT_OFF happens when"
        b" either the system or charger is disconnected.\r\n"
        b"    SMBus communication: %(smbus)b\r\n"
        b"    Serial number      : %(serial_number)b\r\n"
        b"    EEPROM             : %(eeprom)b\r\n"
        b"\x1b[1;32mdolos:~> \x1b[m"
    )

    def __init__(self):
        self.uptime = b"0 days and 00:00:40 seconds"
        self.charger = b"Detected"
        self.efuse = b"Good"
        self.system_present = b"Present (signal HIGH, polarity HIGH)"
        self.bms_state = b"BMS_STATE_POWER_OUTPUT_ON"
        self.smbus = b"Detected"
        self.serial_number = b"DOLOSV1-C-1520241234"
        self.eeprom = b"Successful"

    def render(self):
        return self.template % {
            b"uptime": self.uptime,
            b"charger": self.charger,
            b"efuse": self.efuse,
            b"system_present": self.system_present,
            b"bms_state": self.bms_state,
            b"smbus": self.smbus,
            b"serial_number": self.serial_number,
            b"eeprom": self.eeprom,
        }


@pytest.fixture(scope="function")
def mock_dolos_console():
    def generate_dolos_console(status=DolosStatus()):
        class MockDolosConsole(contextlib.AbstractContextManager):
            def __init__(self, status=status):
                self.last_write = None
                self.console_responses = {
                    b"": None,
                    b"status": status.render(),
                    b"version": (
                        b"\x1b[1;32mdolos:~> "
                        b"\x1b[m version Dolos version "
                        b"0.135.0-1a5dbd1 "
                        b"\x1b[1;32mdolos:~> \x1b[m"
                    ),
                    b"reset": b"reset\r\n",
                }

            def reset_input_buffer(self):
                pass

            def reset_output_buffer(self):
                pass

            def write(self, text):
                self.last_write = text.strip(
                    f"{CONSOLE_ENTER}{CONSOLE_CLEAR_CHR} ".encode("utf-8")
                )

            def read(self):
                return None

            def read_until(self, unused_end_char):
                return self.console_responses[self.last_write]

            def flush(self):
                pass

            def __exit__(self, exc_type, exc_value, traceback):
                pass

        return MockDolosConsole(status=status)

    return generate_dolos_console
