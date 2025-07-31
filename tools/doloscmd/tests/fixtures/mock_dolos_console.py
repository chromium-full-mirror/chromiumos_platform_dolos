# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring

import contextlib
import logging
import re

from doloscmd.console_lib import CONSOLE_CLEAR
from doloscmd.console_lib import CONSOLE_ENTER
from doloscmd.console_lib import EEPROM_SIZE
from doloscmd.console_lib import EEPROM_WRITE_SIZE
import pytest


_logger = logging.getLogger("mock_dolos_host")


class DolosStatus:  # pylint: disable=too-many-instance-attributes, too-few-public-methods

    template = (
        "status\r\n"
        "Dolos status:\r\n"
        "    Uptime             : %(uptime)s\r\n"
        "    Charger            : %(charger)s\r\n"
        "    E-Fuse Power       : %(efuse)s\r\n"
        "    System present     : %(system_present)s\r\n"
        "    BMS current state  : %(bms_state)s\r\n"
        "    BMS next state     : Next States can be BMS_STATE_POWER_GOOD_WAIT"
        "or BMS_STATE_POWER_OUTPUT_OFF\r\n"
        "                         Transition to BMS_STATE_POWER_GOOD_WAIT happens"
        " when the system and charger are connected and the input voltage is out of "
        "efuse range.\r\n"
        "                         Transition to BMS_STATE_POWER_OUTPUT_OFF happens when"
        " either the system or charger is disconnected.\r\n"
        "    SMBus communication: %(smbus)s\r\n"
        "    Serial number      : %(serial_number)s\r\n"
        "    EEPROM             : %(eeprom)s\r\n"
        "\x1b[1;32mdolos:~> \x1b[m"
    )

    def __init__(self):
        self.uptime = "0 days and 00:00:40 seconds"
        self.charger = "Detected"
        self.efuse = "Good"
        self.system_present = "Present (signal HIGH, polarity HIGH)"
        self.bms_state = "BMS_STATE_POWER_OUTPUT_ON"
        self.smbus = "Detected"
        self.serial_number = "DOLOSV1-C-1520241234"
        self.eeprom = "Successful"

    def status(self, cmd):
        return self.template % {
            "uptime": self.uptime,
            "charger": self.charger,
            "efuse": self.efuse,
            "system_present": self.system_present,
            "bms_state": self.bms_state,
            "smbus": self.smbus,
            "serial_number": self.serial_number,
            "eeprom": self.eeprom,
        }


class DolosEEPROM:
    def __init__(self):
        self.data = bytearray([x % 256 for x in range(EEPROM_SIZE)])
        self.write_cnt = 0

    def readall(self, cmd):
        response = [cmd, "EEPROM data:"]
        resp_format = (
            "{:08X}: " + ("{:02x} " * 8) + " " + ("{:02x} " * 8) + "|........ ........|"
        )
        for addr in range(0, EEPROM_SIZE, 16):
            segment = self.data[addr : addr + 16]
            response.append(resp_format.format(addr, *segment))
        response.append("\x1b[1;32mdolos:~> \x1b[m")
        response = "\r\n".join(response)
        return response

    def writen(self, cmd):
        self.write_cnt += 1
        invalid_length = "Invalid provided data length, expected 1 entries"
        failed_write = "Failed to write data to EEPROM"
        response = [cmd]
        # Command format is:
        # eeprom writen {hex addr} {dec length} {space delim hex bytes}

        args = cmd.split()
        addr = int(args[2], 16)
        length = int(args[3])
        data = [int(x, 16) for x in args[4:]]
        assert len(data) == length and len(data) <= EEPROM_WRITE_SIZE
        for offset, val in enumerate(data):
            self.data[addr + offset] = val
        response.append(f"Wrote data to address={hex(addr)}")
        response.append("\x1b[1;32mdolos:~> \x1b[m")
        response = "\r\n".join(response)
        return response


@pytest.fixture(scope="function")
def mock_dolos_console():
    def generate_dolos_console(status=DolosStatus(), eeprom=DolosEEPROM()):
        class MockDolosConsole(contextlib.AbstractContextManager):
            def __init__(self, status=status, eeprom=eeprom):
                self.last_write = ""
                self.write_history = []
                self.sys_pres_state = "Off"
                self.status = status
                self.eeprom = eeprom
                # Console responses are an ordered lookup. The first
                # result matching is used as the handler.
                self.console_responses = {
                    "\n": lambda x: ("\r\n\x1b[1;32mdolos:~> \x1b[m"),
                    "status": lambda x: self.status.status(x),
                    "version": lambda x: (
                        "version\r\n"
                        "Dolos version 0.135.0-1a5dbd1\r\n"
                        "\x1b[1;32mdolos:~> \x1b[m"
                    ),
                    "reset": lambda x: (
                        "reset\r\n"
                        "Resetting device...\r\n"
                        "\x1b[1;32mdolos:~> \x1b[m"
                    ),
                    "eeprom readall": self.eeprom.readall,
                    "eeprom writen": self.eeprom.writen,
                    "sys_pres on": self.sys_pres,
                    "sys_pres disable": self.sys_pres,
                }

            def reset_input_buffer(self):
                pass

            def reset_output_buffer(self):
                pass

            def write(self, text):
                text = text.decode()
                # Handle the clear text key
                if CONSOLE_CLEAR in text:
                    start = text.rfind(CONSOLE_CLEAR) + len(CONSOLE_CLEAR)
                    self.last_write = text[start:]
                    return
                self.write_history.append(text)
                self.last_write += text

            def read(self):
                return None

            def read_until(self, unused_end_char):
                # Do not return a result until we see a newline.
                if CONSOLE_ENTER not in self.last_write:
                    return b""

                # Trim anything but the last newline from the command
                cmd = self.last_write.strip() + "\n"

                # Return the first command matching the prefix.
                # This allows us to support arguments
                for key, handler in self.console_responses.items():
                    if cmd.startswith(key):
                        response = handler(cmd)
                        return response.encode()
                return b""

            def flush(self):
                pass

            def close(self):
                pass

            def reset_input_buffer(self):
                pass

            def reset_output_buffer(self):
                pass

            def sys_pres(self, state):
                state = state.strip()
                self.sys_pres_state = state
                if state == "sys_pres on":
                    return (
                        "sys_pres on\r\n"
                        "Forcing system to be always present\r\n"
                        "\x1b[1;32mdolos:~> \x1b[m"
                    )
                return (
                    "sys_pres disable\r\n"
                    "Forcing system to be always absent\r\n"
                    "\x1b[1;32mdolos:~> \x1b[m"
                )

            def __exit__(self, exc_type, exc_value, traceback):
                pass

        return MockDolosConsole(status=status)

    return generate_dolos_console
