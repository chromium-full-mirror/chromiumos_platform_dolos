# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from doloscmd.eeprom_layout import EEPROMLayout
from doloscmd.error import DolosConsoleError
from doloscmd.registers import REGISTERS
import pytest


class EEPROMFixture:

    FLASH_SIZE = 1024

    def __init__(self, data=None):
        if data is None:
            data = [x % 256 for x in range(self.FLASH_SIZE)]
        assert len(data) == self.FLASH_SIZE
        self.data = bytes(data)


def create_table():
    table = {"Polarity": "High"}
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
        fixture = EEPROMFixture()
        eeprom = EEPROMLayout(fixture.data)
        assert eeprom.dolos_serial is not None

    def test_detect_invalid_serial(self):
        fixture = EEPROMFixture([5] * EEPROMFixture.FLASH_SIZE)
        eeprom = EEPROMLayout(fixture.data)
        assert eeprom.dolos_serial is None

    def test_parse_serial_text(self):
        fixture = EEPROMFixture()
        eeprom = EEPROMLayout(fixture.data)
        text = "DOLOSV1-C-1520240123"
        eeprom.update_serial(text)
        assert eeprom.dolos_serial == {
            "text": text,
            "version": 1,
            "year": 24,
            "week": 15,
            "number": 123,
        }

    def test_serial_update(self):
        fixture = EEPROMFixture()
        eeprom = EEPROMLayout(fixture.data)
        start_serial = eeprom.dolos_serial
        text = "DOLOSV1-C-1520240123"
        eeprom.update_serial(text)
        data = eeprom.create_payload(create_table())
        assert start_serial != eeprom.dolos_serial
        assert eeprom.dolos_serial == {
            "text": text,
            "version": 1,
            "year": 24,
            "week": 15,
            "number": 123,
        }

    def test_require_valid_serial(self):
        fixture = EEPROMFixture([5] * EEPROMFixture.FLASH_SIZE)
        eeprom = EEPROMLayout(fixture.data)
        with pytest.raises(DolosConsoleError):
            data = eeprom.create_payload(create_table())
        eeprom.update_serial("DOLOSV1-C-1520240123")
        data = eeprom.create_payload(create_table())

    def test_verify_serial_unchanged(self):
        fixture = EEPROMFixture()
        eeprom_start = EEPROMLayout(fixture.data)
        data = eeprom_start.create_payload(create_table())
        eeprom_end = EEPROMLayout(data)
        assert eeprom_start.dolos_serial == eeprom_end.dolos_serial
