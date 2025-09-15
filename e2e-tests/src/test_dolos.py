# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=redefined-outer-name
# pylint: disable=wrong-import-position
# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=unused-argument

import sys


sys.path.insert(0, "/tools")

import os
import time

import commands
from doloscmd.console_lib import DolosConsole
import pytest


MINIMUM_VERSION_FIRMWARE = "1.228.0-ced081e"
MINIMUM_VERSION_BOOTLOADER = "2.228.0-ced081e"
MINIMUM_VERSION_COMBINED = "3.228.0-ced081e"


@pytest.fixture(scope="session", autouse=True)
def setup():
    """
    Setup fixture to initialize the environment
    """
    commands.servo_client_main.set("top_usb_mux_sel", "servo_sees_usbkey")
    # Sleep to let USB devices properly enumerate
    time.sleep(3)
    # Remove the major version number to get suffix
    target_version_suffix = os.getenv("TARGET_FW_VERSION")[1:]
    if target_version_suffix is None:
        print("\n[ERROR]: TARGET_FW_VERSION environment variable is not set!")
        pytest.exit(
            "Test run aborted due to missing environment variable.", returncode=1
        )
    target_version_firmware = "1" + target_version_suffix
    target_version_bootloader = "2" + target_version_suffix
    target_version_combined = "3" + target_version_suffix

    console = DolosConsole.find_dolos_serial()
    current_version = console.get_version()

    if current_version.startswith("0."):
        print("Legacy version detected, running conversion update...")
        sys.stdout.flush()
        console.update_firmware(MINIMUM_VERSION_COMBINED)
        time.sleep(1)
        console.run_firmware_command("boot", bootloader=True)
        time.sleep(2)
        current_version = console.get_version()
        if current_version != MINIMUM_VERSION_FIRMWARE:
            console.close()
            pytest.exit(
                "Test run aborted. Failed to update to minimum firmware version.",
                returncode=1,
            )

    if current_version != target_version_firmware:
        print("Updating to target version...")
        sys.stdout.flush()
        console.update_firmware(target_version_combined)
        time.sleep(1)
        console.run_firmware_command("boot", bootloader=True)
        time.sleep(2)
        post_update_version_firmware = console.get_version()
        if post_update_version_firmware != target_version_firmware:
            console.close()
            pytest.exit(
                "Test run aborted. Failed to update to target firmware version.",
                returncode=1,
            )
        post_update_version_bootloader = console.get_version(bootloader=True)
        if post_update_version_bootloader != target_version_bootloader:
            console.close()
            pytest.exit(
                "Test run aborted. Failed to update to target bootloader version.",
                returncode=1,
            )

    console.close()
    # Let the firmware stabilize
    time.sleep(2)


@pytest.fixture
def console():
    """Pytest fixture to set up and tear down the DolosConsole."""
    dolos_console = DolosConsole.find_dolos_serial()

    yield dolos_console

    print("\nClosing console connection.")
    dolos_console.close()
    del dolos_console
    time.sleep(3)


@pytest.mark.parametrize("execution_count", range(5))
def test_powercycle(console, execution_count):
    """
    Test for powercycle
    """
    preboot_uptime = console.get_uptime()
    if preboot_uptime is None:
        pytest.fail("Unable to get uptime from Dolos")

    console.close()

    commands.dolos_powercycle()
    console.open()

    time.sleep(3)
    console.run_firmware_command("boot", bootloader=True)

    time.sleep(3)
    postboot_uptime = console.get_uptime()
    if postboot_uptime is None:
        pytest.fail("Unable to get uptime from Dolos")

    # This condition is assured by a 3 second sleep in
    # the fixture after each test, so a failure of this test
    # means a failure of the firmware, not of the environment.
    assert postboot_uptime <= preboot_uptime
