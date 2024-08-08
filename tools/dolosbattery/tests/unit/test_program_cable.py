# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dolosbattery.layout import REGISTERS
from dolosbattery.program_cable import Flash
from dolosbattery.program_cable import ProgramCableException
import pytest


class EEPROMFixture:

    FLASH_SIZE = 1024

    def __init__(self, data=None):
        if data is None:
            data = [x % 256 for x in range(self.FLASH_SIZE)]
        assert len(data) == self.FLASH_SIZE
        self.data = bytes(data)

    def read(self):
        return self.data

    def write(self, data):
        assert len(data) == self.FLASH_SIZE
        assert isinstance(data, bytes)
        self.data = data


def create_table():
    table = {"PolarityHigh": True}
    for reg in REGISTERS:
        value = reg.get("word", None)
        if value is not None:
            value *= 100
        else:
            index = reg.get("block", None)
            value = list(range(index, 10 + 2 * index))
        table[reg["name"]] = value
    return table


def read_block_reg(data, index):
    offset = 256
    size = 33
    start = offset + (index * size)
    segment = data[start : start + size]
    if segment[0] == 0:
        return None
    return (segment[2] << 8) + segment[1]


def read_word_reg(data, index):
    offset = 512
    size = 3
    start = offset + (index * size)
    segment = data[start : start + size]
    length = segment[0]
    return list(segment[1 : length + 1])


class TestProgramCableFlash:
    def test_valid_serial(self):
        eeprom = EEPROMFixture()
        flash = Flash(eeprom)
        flash.read()
        assert flash.dolos_serial is not None

    def test_detect_invalid_serial(self):
        eeprom = EEPROMFixture([5] * EEPROMFixture.FLASH_SIZE)
        flash = Flash(eeprom)
        flash.read()
        assert flash.dolos_serial is None

    def test_parse_serial_text(self):
        eeprom = EEPROMFixture()
        flash = Flash(eeprom)
        flash.read()
        text = "DOLOSV1-C-1520240123"
        flash.update_serial(text)
        assert flash.dolos_serial == {
            "text": text,
            "version": 1,
            "year": 24,
            "week": 15,
            "number": 123,
        }

    def test_serial_update(self):
        eeprom = EEPROMFixture()
        flash = Flash(eeprom)
        flash.read()
        start_serial = flash.dolos_serial
        text = "DOLOSV1-C-1520240123"
        flash.update_serial(text)
        data = flash.create_payload(create_table())
        flash.write(data)
        flash.read()
        assert start_serial != flash.dolos_serial
        assert flash.dolos_serial == {
            "text": text,
            "version": 1,
            "year": 24,
            "week": 15,
            "number": 123,
        }

    def test_require_valid_serial(self):
        eeprom = EEPROMFixture([5] * EEPROMFixture.FLASH_SIZE)
        flash = Flash(eeprom)
        flash.read()
        with pytest.raises(ProgramCableException):
            data = flash.create_payload(create_table())
        flash.update_serial("DOLOSV1-C-1520240123")
        data = flash.create_payload(create_table())
        flash.write(data)
        flash.read()

    def test_verify_serial_unchanged(self):
        eeprom = EEPROMFixture()
        flash = Flash(eeprom)
        flash.read()
        start_serial = flash.dolos_serial
        data = flash.create_payload(create_table())
        flash.write(data)
        flash.read()
        assert start_serial == flash.dolos_serial

    def test_verify_data_blocks(self):
        eeprom = EEPROMFixture()
        flash = Flash(eeprom)
        flash.read()
        data = flash.create_payload(create_table())
        print("read_word_reg")
        read_word_reg(data, 3)
        print("read_block_reg")
        read_block_reg(data, 3)
        print("read_done")
        assert False
