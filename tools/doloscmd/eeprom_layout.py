# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities and descriptors of the battery registers."""

import binascii
import logging
import re
import struct

from doloscmd import registers
from doloscmd.error import DolosConsoleError


class EEPROMLayout:
    """Handles the Dolos Cable Flash."""

    def __init__(self, data):
        self.data = data
        self._dolos_serial = None
        self._parse_data()

    def _parse_data(self):
        """Read the cable and extract it's the serial."""
        serial_bytes = self.data[0:6]

        # A uniform serial is likely invalid and should not be used to create
        # a new one. This provides a protection against silent I2C read failures
        # causing a cable to lose it's serial.
        if min(serial_bytes) == max(serial_bytes):
            logging.warning("Invalid serial")
            self._dolos_serial = None
        else:
            version, _, year, week, number = struct.unpack("<BBBBH", serial_bytes)
            self._dolos_serial = {
                "version": version,
                "year": year,
                "week": week,
                "number": number,
            }
            logging.info("Serial %r", self.dolos_serial["text"])

        return self.data

    def update_serial(self, text):
        """Converts the serial number from a string into the flash format.

        A serial number format looks like the following: DOLOSV1-C-1520240123
        The last segment is extracted to the following WWYYYYNNNN
        """
        text = text.upper()
        match = re.search(r"DOLOSV(\d+)-.*-([\d]{10,11})$", text)
        if not match:
            raise DolosConsoleError(f"Invalid serial format: {text}")
        version = int(match[1])
        segment = match[2]
        week = int(segment[:2])
        year = int(segment[2:6])
        number = int(segment[6:])

        if week > 53:
            raise DolosConsoleError(f"Invalid week number: {week} in serial {text}")
        if year < 2024:
            raise DolosConsoleError(f"Invalid year number: {year} in serial {text}")
        year -= 2000
        self._dolos_serial = {
            "version": version,
            "year": year,
            "week": week,
            "number": number,
        }

    @property
    def dolos_serial(self):
        """Returns the serial attributes and generates a text form."""
        if self._dolos_serial:
            # pylint: disable=consider-using-f-string
            text = "DOLOSV{version}-C-{week}20{year}{number:04d}".format(
                **self._dolos_serial
            )
            ret = {"text": text}
            ret |= self._dolos_serial
            return ret
        return None

    def create_payload(self, table):
        """Create the binary payload from the config table and persistent fields.

        Args:
            table: Configuration table

        Returns:
            Bytes: Binary data to write to the eeprom as a 1024B

        Raises:
            DolosConsoleError: We are missing critical data
        """

        data_section = self._create_data_section(table)

        polarity = None
        polarity_setting = table.get("Polarity")
        if isinstance(polarity_setting, str):
            polarity_setting = polarity_setting.lower()
            polarity_map = {
                "low": 0,
                "high": 1,
                "float": 2,
                "unknown": 0,
            }
            if polarity_setting == "unknown":
                logging.warning("Polarity is 'unknown', using default")
            polarity = polarity_map.get(polarity_setting)
        else:
            logging.error("'Polarity' must be set to a string")
        if polarity is None:
            raise DolosConsoleError(
                "'Polarity' field invalid in yaml. Options: 'unknown', 'low', 'high', 'float'"
            )

        if self.dolos_serial is None:
            raise DolosConsoleError("Serial failed to load")

        crc = binascii.crc32(data_section) ^ 0xFFFFFFFF
        flash_data = struct.pack(
            "<BBBBHI1014s",
            self.dolos_serial["version"],
            polarity,
            self.dolos_serial["year"],
            self.dolos_serial["week"],
            self.dolos_serial["number"],
            crc,
            data_section,
        )
        return flash_data

    def _create_data_section(self, table):
        """Write the the data fields in the eeprom_data struct format.

        This generates all fields used following the CRC calculation.
        This includes the 2 data sections and the reserved padding.

        Args:
            table: Table of registers

        Returns:
            bytes: 1014B binary representation of the struct fields.
        """
        sb_total_block_regs = 7
        sb_total_word_regs = 170
        block_registers = [self._write_block(None)] * sb_total_block_regs
        word_registers = [self._write_word(None)] * sb_total_word_regs

        for field in registers.REGISTERS:
            name = field["name"]
            value = table.get(name)
            if value is None:
                if "opt" not in field:
                    logging.error("Required field %s missing", name)
                else:
                    continue
            if "block" in field:
                block_registers[field["block"]] = self._write_block(value)
            else:
                word_registers[field["word"]] = self._write_word(value)
        data_section = bytes(246)
        data_section += b"".join(block_registers)
        data_section += bytes(25)
        data_section += b"".join(word_registers)
        data_section += bytes(2)
        return data_section

    def _write_block(self, data):
        """Write the the data as the sb_block_register struct format

        sb_block_register includes a int describing the length of the payload
        which is 0 when 'no data' and a payload section of 32 bytes.

        Args:
            data: List of byte values under 32B long.
                  If None returns the 'no data' representation

        Returns:
            bytes: 33B binary representation of the struct.
        """
        if data is None:
            return bytes(33)
        if isinstance(data, str):
            data = data.encode()
        data = struct.pack("<B32s", len(data), bytes(data))
        return data + bytes(33 - len(data))

    def _write_word(self, data):
        """Write the the data as the sb_word_register struct format.

        sb_word_register has a present bit and data stored in uint16_t.

        Args:
            data: Integer value in the range of uint16_t.
                  If None, returns the 'no data' representation.

        Returns:
            bytes: 3B binary representation of the struct.
        """
        if data is None:
            return bytes(3)
        return struct.pack("<BH", 1, data)
