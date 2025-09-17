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
import utils


MINIMUM_VERSION_FIRMWARE = "1.228.0-ced081e"
MINIMUM_VERSION_BOOTLOADER = "2.228.0-ced081e"
MINIMUM_VERSION_COMBINED = "3.228.0-ced081e"


@pytest.fixture(scope="session", autouse=True)
def setup():
    """
    Setup fixture to initialize the environment once per test session.
    Expected behaviour:
     - Sets up the hardware environment (USB mux).
     - Checks for the mandatory TARGET_FW_VERSION environment variable.
     - Detects if the device is running a legacy firmware (version 0.x).
     - If legacy, performs a one-time 'conversion update' to a baseline version.
     - Ensures the device is in a known, non-legacy state before the
       test suite begins.
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

    console = DolosConsole.find_dolos_serial()
    current_version = console.get_version()

    if current_version.startswith("0."):
        print("[INFO]: Legacy version detected, running conversion update...")
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

    console.close()
    # Let the firmware stabilize
    time.sleep(2)


@pytest.fixture
def console():
    """
    Pytest fixture to set up and tear down the DolosConsole for each test.
    Expected behaviour:
     - (Setup) Before each test, it connects to the Dolos console.
     - (Setup) It checks if the running FW and bootloader versions match the
       target versions specified by the environment variable.
     - (Setup) If there is a mismatch, it updates the FW and/or bootloader
       to the target version.
     - (Setup) It yields an active console object to the test function.
     - (Teardown) After each test, it closes the console connection and pauses
       to ensure a clean state for the next test.
    """

    # Pre test setup
    dolos_console = DolosConsole.find_dolos_serial()

    target_version_suffix = os.getenv("TARGET_FW_VERSION")[1:]
    target_version_firmware = "1" + target_version_suffix
    target_version_bootloader = "2" + target_version_suffix

    firmware_version = dolos_console.get_version()
    bootloader_version = dolos_console.get_version(bootloader=True)

    if firmware_version is None or bootloader_version is None:
        pytest.fail("[ERROR]: Unable to retrieve version strings.")

    if firmware_version != target_version_firmware:
        print(
            "[INFO]: Running firmware version different from target. "
            "Running update..."
        )
        sys.stdout.flush()
        dolos_console.update_firmware(target_version_firmware)
        time.sleep(1)
        dolos_console.run_firmware_command("boot", bootloader=True)
        time.sleep(3)

    if bootloader_version != target_version_bootloader:
        print(
            "[INFO]: Running bootloader version different from target."
            " Running update..."
        )
        sys.stdout.flush()
        dolos_console.update_bootloader(target_version_bootloader)
        time.sleep(1)
        dolos_console.run_firmware_command("boot", bootloader=True)
        time.sleep(3)

    yield dolos_console

    # Post test cleanup
    dolos_console.close()
    del dolos_console
    sys.stdout.flush()
    time.sleep(3)


@pytest.mark.parametrize("execution_count", range(5))
def test_powercycle(console, execution_count):
    """
    Test for powercycle.
    Expected behaviour:
     - Get uptime from FW before powercycle
     - Powercycle Dolos and wait for bootloader
     - Boot into FW from bootloader
     - Get uptime from FW after powercycle
     - Uptime after powercycle should be less than before
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


def test_update_firmware(console):
    """
    Test the update process by updating the firmware to the latest version
    which is different from the one currently running.
    Expected behaviour:
     - Read current FW version
     - Find a different, available FW version to update to
     - Run the update process
     - Boot into the newly updated FW
     - The new FW version should match the target version and be
       different from the pre-test version
    """
    pretest_version = console.get_version()

    versions = utils.get_firmware_versions()

    assert len(versions) >= 2

    # Since versions are sorted descending, assume latest is
    # different from running version, else use the next one
    target_version = versions[0]
    if pretest_version == target_version:
        target_version = versions[1]

    console.update_firmware(target_version)
    console.run_firmware_command("boot", bootloader=True)

    time.sleep(3)

    posttest_version = console.get_version()

    assert pretest_version != posttest_version
    assert posttest_version == target_version


def test_update_bootloader(console):
    """
    Test the update process by updating the bootloader to the latest version
    which is different from the one currently running.
    Expected behaviour:
     - Read current bootloader version from FW
     - Find a different, available bootloader version to update to
     - Run the update process
     - The bootloader should report its new version correctly
     - Boot into FW
     - The FW should report the new bootloader version, which should
       match the target version
    """
    pretest_bootloader_version = console.get_version(bootloader=True)
    pretest_firmware_version = console.get_version()

    versions = utils.get_bootloader_versions()

    assert len(versions) >= 2

    # Since versions are sorted descending, assume latest is
    # different from running version, else use the next one
    target_version = versions[0]
    if pretest_bootloader_version == target_version:
        target_version = versions[1]

    assert target_version is not None

    console.update_bootloader(target_version)
    time.sleep(1)
    # Also query bootloader for its version
    version_from_bootloader = console.run_firmware_command("version", bootloader=True)
    console.run_firmware_command("boot", bootloader=True)

    time.sleep(3)

    posttest_bootloader_version = console.get_version(bootloader=True)
    posttest_firmware_version = console.get_version()

    assert pretest_bootloader_version != posttest_bootloader_version
    assert posttest_bootloader_version == target_version
    # Bootloader prints version as "Bootloader version 2.x"
    # so check if version is substring not equality
    assert posttest_bootloader_version in version_from_bootloader
    # Check that bootloader update does not affect firmware
    assert pretest_firmware_version == posttest_firmware_version


def test_bootloader_timeout(console):
    """
    Test the timeout feature of the bootloader.
    Expected behaviour:
     - Dolos boots into bootloader after FW reset command
     - Bootloader console is responsive
     - Bootloader, after 10s of inactivity, should time out and boot into FW
     - Able to read FW uptime, this should be lower than pre-test uptime
    """
    # Extra delay to make sure the preboot_uptime is
    # greater than after timeout
    time.sleep(5)

    preboot_uptime = console.get_uptime()

    console.run_firmware_command("reset")
    console.close()
    time.sleep(1)
    console.open()

    # Test that bootloader console is up
    invalid_response = console.run_firmware_command("gibberish", bootloader=True)
    assert "Unrecognized command" in invalid_response

    # Give the bootloader enough time to timeout
    time.sleep(12)

    postboot_uptime = console.get_uptime()
    assert postboot_uptime <= preboot_uptime


def test_bootloader_help(console):
    """
    Test the help command of the bootloader console.
    Expected behaviour:
     - Dolos boots into bootloader after FW reset command
     - The 'help' command returns a list of available commands
     - The command list should contain expected commands like 'boot' and 'reset'
     - The device can successfully boot back into FW
    """
    # Reset to enter bootloader console
    console.run_firmware_command("reset")
    console.close()
    time.sleep(1)
    console.open()
    help_response = console.run_firmware_command("help", bootloader=True)
    console.run_firmware_command("boot", bootloader=True)

    # Check some of the expected commands are present
    assert "--" in help_response
    assert "boot" in help_response
    assert "reset" in help_response
    assert "bsl" in help_response
    assert "version" in help_response
