"""Test suite for factory"""

# Lint as: python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time


def test_pac(fw_cmd):
    """This is the pac test.

    Tests the voltage and current of the Dolos.
    Current should be greater than 0 and voltage
    should be greater than or equal to 8.
    """
    min_expected_vol = 8
    min_cur = 0
    actual_voltage, actual_current = fw_cmd.read_pac()
    assert (
        actual_current > min_cur and actual_voltage >= min_expected_vol
    ), f"{actual_voltage}mv and {actual_current}ma observed"


def test_buck_boost(fw_cmd):
    """This is the buck boost test.

    Tests the buck boost by reading the status of
    chrg_ok pin and reading the bytes value from buck boost.
    chrg_ok should be 1 and bytes should be 0x40.
    """
    expected_chrg_ok_value = 1
    expected_byte_value = "0x40"
    (
        actual_byte_value,
        actual_chrg_ok_value,
    ) = fw_cmd.read_buck_boost_and_chrg_pin()
    assert (
        actual_chrg_ok_value == expected_chrg_ok_value
    ), f"{actual_chrg_ok_value} is the chrg_ok pin state observed"
    assert actual_byte_value == expected_byte_value, (
        f"Value read from buck boost is: "
        f"{actual_byte_value}, but it should be: {expected_byte_value}"
    )


def test_eeprom(fw_cmd):
    """This is the eeprom test.

    Tests the eeprom by calling the read_eeprom function,
    if there is no ERROR in the response then the
    test will pass.
    """
    output = fw_cmd.read_eeprom()
    error_present = any("ERROR" in s for s in output)
    assert not error_present, "Error reading the eeprom"


def test_smbus(fw_cmd):
    """This is the SMBus Test.

    Tests the smbus by taking two screenshots of
    stats smbus command response, then comparing the values
    of smbus read success, smart battery read fail
    and smart battery write fail between these two screenshots.
    the second smbus read success value should be greater
    than the first one and all the failures values
    should be 0.
    """
    expected_diff, expected_failures = 0, 0
    (
        first_smbus_read_success_value,
        first_smart_battery_read_fail_value,
        first_smart_battery_write_fail_value,
    ) = fw_cmd.read_smbus_stats()
    time.sleep(1)
    (
        second_smbus_read_success_value,
        second_smart_battery_read_fail_value,
        second_smart_battery_write_fail_value,
    ) = fw_cmd.read_smbus_stats()
    assert (
        first_smart_battery_read_fail_value == expected_failures
        and second_smart_battery_read_fail_value == expected_failures
    ), (
        f"{first_smart_battery_read_fail_value} smart battery"
        f" read failures observed"
    )
    assert (
        first_smart_battery_write_fail_value == expected_failures
        and second_smart_battery_write_fail_value == expected_failures
    ), (
        f"{second_smart_battery_write_fail_value}smart battery"
        f" write failures observed"
    )
    actual_diff = second_smbus_read_success_value - first_smbus_read_success_value
    assert (
        actual_diff > expected_diff
    ), f"{actual_diff} is the difference between the number of reads"


def test_temp(fw_cmd):
    """This is the temperature test.

    Tests the temperature by reading the temperature in Celsius.
    It should be greater than or equal to 5 and smaller than or equal to 40.
    """
    min_expected_temp = 5
    max_expected_temp = 40
    actual_temp_in_cel = fw_cmd.read_temp()
    assert (
        min_expected_temp <= actual_temp_in_cel <= max_expected_temp
    ), f"{actual_temp_in_cel} celsius observed"


def test_efuse(fw_cmd):
    """This is the efuse test.

    Reads the efuse pin status value. It should be equal to 1.
    """
    expected_efuse_value = 1
    actual_efuse_value = fw_cmd.read_efuse_pg_pin()
    assert (
        actual_efuse_value == expected_efuse_value
    ), f"{actual_efuse_value} is the efuse-pg pin value observed"
