# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities and descriptors of the battery registers."""

REGISTERS = [
    {"reg": 0x00, "word": 0, "name": "SB_MANUFACTURER_ACCESS", "opt": True},
    {"reg": 0x01, "word": 1, "name": "SB_REMAINING_CAPACITY_ALARM"},
    {"reg": 0x02, "word": 2, "name": "SB_REMAINING_TIME_ALARM"},
    {"reg": 0x03, "word": 3, "name": "SB_BATTERY_MODE"},
    {"reg": 0x04, "word": 4, "name": "SB_AT_RATE", "const": False},
    {"reg": 0x05, "word": 5, "name": "SB_AT_RATE_TIME_TO_FULL", "const": False},
    {"reg": 0x06, "word": 6, "name": "SB_AT_RATE_TIME_TO_EMPTY", "const": False},
    {"reg": 0x07, "word": 7, "name": "SB_AT_RATE_OK", "const": False},
    {"reg": 0x08, "word": 8, "name": "SB_TEMPERATURE", "const": False},
    {"reg": 0x09, "word": 9, "name": "SB_VOLTAGE", "const": False},
    {"reg": 0x0A, "word": 10, "name": "SB_CURRENT", "const": False},
    {"reg": 0x0B, "word": 11, "name": "SB_AVERAGE_CURRENT", "const": False},
    {"reg": 0x0C, "word": 12, "name": "SB_MAX_ERROR"},
    {"reg": 0x0D, "word": 13, "name": "SB_RELATIVE_STATE_OF_CHARGE", "const": False},
    {"reg": 0x0E, "word": 14, "name": "SB_ABSOLUTE_STATE_OF_CHARGE", "const": False},
    {"reg": 0x0F, "word": 15, "name": "SB_REMAINING_CAPACITY", "const": False},
    {"reg": 0x10, "word": 16, "name": "SB_FULL_CHARGE_CAPACITY"},
    {"reg": 0x11, "word": 17, "name": "SB_RUN_TIME_TO_EMPTY", "const": False},
    {"reg": 0x12, "word": 18, "name": "SB_AVERAGE_TIME_TO_EMPTY", "const": False},
    {"reg": 0x13, "word": 19, "name": "SB_AVERAGE_TIME_TO_FULL", "const": False},
    {"reg": 0x14, "word": 20, "name": "SB_CHARGING_CURRENT", "const": False},
    {"reg": 0x15, "word": 21, "name": "SB_CHARGING_VOLTAGE", "const": False},
    {"reg": 0x16, "word": 22, "name": "SB_BATTERY_STATUS"},
    {"reg": 0x17, "word": 23, "name": "SB_CYCLE_COUNT", "const": True},
    {"reg": 0x18, "word": 24, "name": "SB_DESIGN_CAPACITY", "const": True},
    {"reg": 0x19, "word": 25, "name": "SB_DESIGN_VOLTAGE", "const": True},
    {"reg": 0x1A, "word": 26, "name": "SB_SPECIFICATION_INFO", "const": True},
    {"reg": 0x1B, "word": 27, "name": "SB_MANUFACTURE_DATE", "const": True},
    {"reg": 0x1C, "word": 28, "name": "SB_SERIAL_NUMBER", "const": True},
    {"reg": 0x20, "block": 0, "name": "SB_MANUFACTURER_NAME", "const": True},
    {"reg": 0x21, "block": 1, "name": "SB_DEVICE_NAME", "const": True},
    {"reg": 0x22, "block": 2, "name": "SB_DEVICE_CHEMISTRY", "const": True},
    {"reg": 0x23, "block": 3, "name": "SB_MANUFACTURER_DATA", "const": True},
    {"reg": 0x44, "block": 4, "name": "SB_ALT_MANUFACTURER_ACCESS", "opt": True},
    {"reg": 0x70, "block": 5, "name": "SB_MANUFACTURE_INFO", "opt": True},
    {"reg": 0x3C, "word": 0x3C, "name": "SB_OPTIONAL_MFG_FUNC1", "opt": True},
    {"reg": 0x3D, "word": 0x3D, "name": "SB_OPTIONAL_MFG_FUNC2", "opt": True},
    {"reg": 0x3E, "word": 0x3E, "name": "SB_OPTIONAL_MFG_FUNC3", "opt": True},
    {"reg": 0x3F, "word": 0x3F, "name": "SB_OPTIONAL_MFG_FUNC4", "opt": True},
    {"reg": 0x43, "word": 0x43, "name": "SB_PACK_STATUS", "opt": True},
]


def find_register(name):
    """Find the battery register and it's parameters."""
    return [x for x in REGISTERS if x["name"] == name][0]
