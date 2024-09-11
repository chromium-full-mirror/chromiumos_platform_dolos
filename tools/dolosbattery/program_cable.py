#!/usr/bin/python3
# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a command line interface programming Dolos cable registers."""

import argparse
import binascii
import logging
import pathlib
import re
import struct
import sys

import yaml

from dolosbattery import i2c_interface, layout, logs
from doloscmd import console_lib


class ProgramCableException(Exception):
    """General exception errors when programming the cable."""


def load_table(file):
    """Loads the yaml describing the battery cable configuration.

    Args:
        file (pathlib.Path): File to load

    Returns:
        dict: Cable configuration

    Raises:
        ProgramCableException: Error reading or parsing the yaml file
    """
    logging.info("Loading yaml: %r", file)
    try:
        text = file.read_text()
        table = yaml.safe_load(text)
    except (FileNotFoundError, yaml.scanner.ScannerError, yaml.YAMLError) as err:
        raise ProgramCableException("Unable to read Yaml file %s", file) from err
    return table


class FTDIProgrammer:
    """Handles the EEPROM read and write sequences for Dolos Cables."""

    BASE_ADDR7 = 0x50
    PAGE_SIZE = 16
    FLASH_SIZE = 1024

    def __init__(self, i2c):
        self._i2c = i2c

    def _addr(self, flash_addr):
        """Calculate the correct I2C address and packet header for the address.

        The EEPROM packs the flash address information into the I2C address and
        first byte. This calculates the pair.

        Args:
            flash_addr: Flash address for the start of the read or write operation.

        Returns:
            (int, bytes): I2C address and first byte of the transfer.
        """
        high = flash_addr // 256
        low = flash_addr % 256
        addr7 = self.BASE_ADDR7 + high
        return addr7, bytes([low])

    def _write_page(self, flash_addr, write_data):
        """Write data to the flash address.

        Args:
            flash_addr: Flash address for first byte.
            write_data: Write payload.

        Raises:
            I2CException: Transfer failed.
        """
        addr7, header = self._addr(flash_addr)
        write_data = header + write_data
        self._i2c.transfer(write_data, 0, addr7=addr7)

    def _read_page(self, flash_addr, read_len):
        """Read from the flash.

        Args:
            flash_addr: Flash address for first byte.
            read_len: Number of bytes to read.

        Raises:
            I2CException: Transfer failed.
        """
        addr7, header = self._addr(flash_addr)
        return self._i2c.transfer(header, read_len, addr7=addr7)

    def _read(self):
        """Read all of the flash and return it.

        Splits the read operation up and handles the individual transactions
        while combining the final result.

        Returns:
            bytes: 1024B data

        Raises:
            I2CException: Transfer failed.
        """
        data = bytes()
        for i in range(0, self.FLASH_SIZE, self.PAGE_SIZE):
            segment = self._read_page(i, self.PAGE_SIZE)
            data += segment
        return data

    def _write(self, data):
        """Write all of the flash

        Splits the write operation up and handles the individual transactions.

        Args:
            data (bytes): 1024B flash data.

        Raises:
            ProgramCableException: Invalid data format.
            I2CException: Transfer failed.
        """
        if len(data) != self.FLASH_SIZE:
            raise ProgramCableException("Flash size is invalid")

        for i in range(0, self.FLASH_SIZE, self.PAGE_SIZE):
            segment = data[i : i + self.PAGE_SIZE]
            self._write_page(i, segment)

    def read(self):
        """Read all flash.

        Returns:
            bytes (bytes): 1024B bytes flash data.

        Raises:
            ProgramCableException: Read failed
        """
        try:
            return self._read()
        except i2c_interface.I2CException as err:
            raise ProgramCableException("Unable to read from flash") from err

    def write(self, data):
        """Write the data to the flash.

        Args:
            data (bytes): 1024B flash data.

        Raises:
            ProgramCableException: [description]
        """
        try:
            self._write(data)
        except i2c_interface.I2CException as err:
            raise ProgramCableException("Unable to write flash") from err


class DolosProgrammer:
    """Wraps the doloscmd flash interface."""

    def __init__(self, console):
        self._console = console

    def read(self):
        """Read all flash.

        Returns:
            bytes (bytes): 1024B bytes flash data.

        Raises:
            ProgramCableException: Read failed
        """
        try:
            return self._console.eeprom_read()
        except console_lib.DolosConsoleException as err:
            raise ProgramCableException("Unable to Read flash") from err

    def write(self, data):
        """Write the data to the flash.

        Args:
            data (bytes): 1024B flash data to write

        Raises:
            ProgramCableException: Write failed
        """
        try:
            self._console.eeprom_write(data)
        except console_lib.DolosConsoleException as err:
            raise ProgramCableException("Unable to write flash") from err


class Flash:
    """Handles the Dolos Cable Flash."""

    def __init__(self, programmer):
        self.programmer = programmer
        self.data = b""
        self._dolos_serial = None

    def read(self):
        """Read the cable and extract it's the serial."""
        logging.info("Reading cable")
        self.data = self.programmer.read()

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

    def write(self, data):
        """Write and verify the cable has been programmed successfully."""
        logging.info("Programming cable")
        if self.data == data:
            logging.info("Cable is already programmed")
            return True
        self.programmer.write(data)
        logging.info("Verifying cable")
        self.read()
        if self.data == data:
            logging.info("Program success")
            return True
        logging.info("Program failed")
        return False

    def update_serial(self, text):
        """Converts the serial number from a string into the flash format.

        A serial number format looks like the following: DOLOSV1-C-1520240123
        The last segment is extracted to the following WWYYYYNNNN
        """
        text = text.upper()
        match = re.search(r"DOLOSV(\d+)-.*-([\d]{10,11})$", text)
        if not match:
            raise ProgramCableException(f"Invalid serial format: {text}")
        version = int(match[1])
        segment = match[2]
        week = int(segment[:2])
        year = int(segment[2:6])
        number = int(segment[6:])

        if week > 53:
            raise ProgramCableException(f"Invalid week number: {week} in serial {text}")
        if year < 2024:
            raise ProgramCableException(f"Invalid year number: {year} in serial {text}")
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
            ProgramCableException: We are missing critical data
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
            raise ProgramCableException(
                "'Polarity' field invalid in yaml. Options: 'unknown', 'low', 'high', 'float'"
            )

        if self.dolos_serial is None:
            raise ProgramCableException("Serial failed to load")

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

        for field in layout.REGISTERS:
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


def program_cable(programmer, table, new_serial=None):
    """Program the cable.

    Loads the cable and extracts it's serial and replace if needed.
    Use the serial and table to generate the new binary and program it.

    Args:
        programmer: Cable programmer using either FTDI or Dolos for I2C
        table: Table of register values
        new_serial: Optional new cable serial

    Returns:
        bool: True if successful

    Raises:
        ProgramCableException: Error completing the operation
    """
    flash = None
    if isinstance(programmer, i2c_interface.I2CFTDI):
        flash = Flash(FTDIProgrammer(programmer))
    elif isinstance(programmer, console_lib.DolosConsole):
        flash = Flash(DolosProgrammer(programmer))
    else:
        raise ProgramCableException(f"No valid programmer: {programmer}")
    flash.read()
    if new_serial:
        flash.update_serial(new_serial)
    data = flash.create_payload(table)
    flash.write(data)
    return True


def main():
    """Parse the args, load the table and program the cable.

    Raises:
        ProgramCableException() with diagnostic details on failure.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-y",
        dest="yaml",
        type=pathlib.Path,
        required=True,
        help="File path of imported yaml data",
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "-d",
        dest="dolos",
        type=str,
        help="Use Dolos to program: 'any' is used if the serial number does not matter.",
    )
    group.add_argument(
        "-f",
        dest="ftdi",
        action="store_true",
        help="Use FTDI to program: Fails if more than 1 device is found.",
    )
    parser.add_argument(
        "-ns", dest="new_serial", type=str, help="Optional overwrite serial number"
    )
    parser.add_argument(
        "-v", dest="verbose", action="store_true", help="Enable verbose logging"
    )
    args = parser.parse_args()
    logs.set_config(args.verbose)

    table = load_table(args.yaml)

    programmer = None

    if args.ftdi:
        try:
            programmer = i2c_interface.I2CFTDI()
        except i2c_interface.I2CException as err:
            raise ProgramCableException(
                "Unable to connect to FTDI, is it connected?"
            ) from err
    elif args.dolos:
        try:
            dolos_serial = args.dolos.upper()
            if dolos_serial == "ANY":
                dolos_serial = None
            programmer = console_lib.DolosConsole.find_dolos_serial(dolos_serial)
        except console_lib.DolosConsoleException as err:
            raise ProgramCableException(
                "Unable connect to Dolos, is the serial correct and connected?"
            ) from err

    program_cable(programmer, table, args.new_serial)


if __name__ == "__main__":
    main()
