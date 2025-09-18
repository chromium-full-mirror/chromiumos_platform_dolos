# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=redefined-outer-name
# pylint: disable=wrong-import-position
# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=unused-argument
# pylint: disable=import-error

import sys


sys.path.insert(0, "/tools")

import os
import time

import commands
from doloscmd.console_lib import DolosConsole
from doloscmd.error import DolosConsoleNoEchoError
import pytest
import utils


MINIMUM_VERSION_FIRMWARE = "1.228.0-ced081e"
MINIMUM_VERSION_BOOTLOADER = "2.228.0-ced081e"
MINIMUM_VERSION_COMBINED = "3.228.0-ced081e"

BOOTLOADER_TIMEOUT = 10
BOOTLOADER_TIMEOUT_EXTRA = BOOTLOADER_TIMEOUT + 2
DELAY_VERY_SMALL = 1
DELAY_SMALL = 2
DELAY_MEDIUM = 3
DELAY_LARGE = 5


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
    time.sleep(DELAY_MEDIUM)
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
        time.sleep(DELAY_VERY_SMALL)
        console.run_firmware_command("boot", bootloader=True)
        time.sleep(DELAY_SMALL)
        current_version = console.get_version()
        if current_version != MINIMUM_VERSION_FIRMWARE:
            console.close()
            pytest.exit(
                "Test run aborted. Failed to update to minimum firmware version.",
                returncode=1,
            )

    console.close()
    # Let the firmware stabilize
    time.sleep(DELAY_SMALL)


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
        time.sleep(DELAY_VERY_SMALL)
        dolos_console.run_firmware_command("boot", bootloader=True)
        time.sleep(DELAY_MEDIUM)

    if bootloader_version != target_version_bootloader:
        print(
            "[INFO]: Running bootloader version different from target."
            " Running update..."
        )
        sys.stdout.flush()
        dolos_console.update_bootloader(target_version_bootloader)
        time.sleep(DELAY_VERY_SMALL)
        dolos_console.run_firmware_command("boot", bootloader=True)
        time.sleep(DELAY_MEDIUM)

    yield dolos_console

    # Post test cleanup
    dolos_console.close()
    del dolos_console
    sys.stdout.flush()
    time.sleep(DELAY_MEDIUM)


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

    time.sleep(DELAY_MEDIUM)
    console.run_firmware_command("boot", bootloader=True)

    time.sleep(DELAY_MEDIUM)
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

    time.sleep(DELAY_MEDIUM)

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
    time.sleep(DELAY_VERY_SMALL)
    # Also query bootloader for its version
    version_from_bootloader = console.run_firmware_command("version", bootloader=True)
    console.run_firmware_command("boot", bootloader=True)

    time.sleep(DELAY_MEDIUM)

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
    time.sleep(DELAY_LARGE)

    preboot_uptime = console.get_uptime()

    console.run_firmware_command("reset")
    console.close()
    time.sleep(DELAY_VERY_SMALL)
    console.open()

    # Test that bootloader console is up
    invalid_response = console.run_firmware_command("gibberish", bootloader=True)
    assert "Unrecognized command" in invalid_response

    # Give the bootloader enough time to timeout
    time.sleep(BOOTLOADER_TIMEOUT_EXTRA)

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
    time.sleep(DELAY_VERY_SMALL)
    console.open()
    help_response = console.run_firmware_command("help", bootloader=True)
    console.run_firmware_command("boot", bootloader=True)

    # Check some of the expected commands are present
    assert "--" in help_response
    assert "boot" in help_response
    assert "reset" in help_response
    assert "bsl" in help_response
    assert "version" in help_response


def test_skip_recovery(console):
    """
    Test that the 'skip recovery' bootloader flag takes effect.
    Expected behaviour:
     - Update the device with a special bootloader build that has the
       'skip recovery' feature enabled.
     - This bootloader should bypass the standard 10-second timeout and
       boot the main firmware almost instantly.
     - The test should be able to communicate with the firmware and read
       its version immediately after the boot.
     - The bootloader is restored to the standard target version after
       the test completes.
    """

    # Since the developer flag is packaged with the bootloader image
    # at build time and no such images are currently stored in
    # Google Cloud we will use a local copy for the time being
    # TODO(b/438110087) Change this test to use a current copy
    skip_enabled_image_path = os.getcwd() + "/../images/2.233.0-acc33f7-skip/boot.txt"

    console.update_bootloader_local(skip_enabled_image_path)
    time.sleep(DELAY_VERY_SMALL)
    version = console.get_version()
    bootloader_version = console.get_version(bootloader=True)

    # If the version commands succeed in such a short time
    # that means the bootloader skipped the timeout.
    assert version is not None
    assert bootloader_version == "2.233.0-acc33f7"

    # Restore bootloader to non skip version.
    # Must do that here manually in case skip enabled image version
    # coincides with the target version, in which case, the fixture
    # won't catch it.
    target_version_suffix = os.getenv("TARGET_FW_VERSION")[1:]
    target_version_bootloader = "2" + target_version_suffix
    console.update_bootloader(target_version_bootloader)

    time.sleep(DELAY_VERY_SMALL)
    console.run_firmware_command("boot", bootloader=True)
    post_update_version = console.get_version(bootloader=True)
    # Check update worked
    assert post_update_version == target_version_bootloader


def test_invalid_crc_recovery(console):
    """
    Test the bootloader's recovery mechanism when faced with
    firmware that has an invalid CRC.
    Expected behaviour:
     - Flash a local FW image known to have an invalid CRC.
     - Device should enter the bootloader.
     - Attempting the 'boot' command should fail and return an "ERROR".
     - The bootloader should *not* time out and boot automatically,
       even after waiting longer than the standard 10s timeout.
     - The device should remain in the bootloader (recovery mode).
     - Initiate recovery by entering BSL mode.
     - Flash a known-good, target FW version via BSL.
     - The device should successfully boot into the new, valid FW.
     - The running FW version should match the target version.
    """
    # Since the CRC32 checksum is packaged with the firmware image
    # at build time and no invalid images are stored in
    # Google Cloud we will use a local copy for the time being
    # TODO(b/438110087) Change this test to use a current copy
    invalid_image_path = os.getcwd() + "/../images/1.233.0-acc33f7-invalid/firmware.txt"

    console.update_firmware_local(invalid_image_path)
    time.sleep(DELAY_VERY_SMALL)

    target_version_suffix = os.getenv("TARGET_FW_VERSION")[1:]
    target_version_firmware = "1" + target_version_suffix
    target_version_bootloader = "2" + target_version_suffix

    bootloader_version = console.run_firmware_command("version", bootloader=True)
    # Bootloader prints version as "Bootloader version 2.x"
    # so check if version is substring not equality
    assert target_version_bootloader in bootloader_version

    invalid_boot_response = console.run_firmware_command("boot", bootloader=True)
    # Boot should throw an error because of failed CRC verification
    assert "ERROR" in invalid_boot_response

    time.sleep(BOOTLOADER_TIMEOUT_EXTRA)
    bootloader_help_response = console.run_firmware_command("help", bootloader=True)
    # Normally bootloader would have timed out, but in case of
    # invalid CRC it stays in recovery mode
    assert "boot_unsafe" in bootloader_help_response

    ### START RECOVERY ###

    console.run_firmware_command("bsl", bootloader=True)
    console.update_firmware(target_version_firmware, bsl_mode=True)

    time.sleep(DELAY_VERY_SMALL)
    console.run_firmware_command("boot", bootloader=True)
    time.sleep(DELAY_VERY_SMALL)
    firmware_version = console.get_version()

    assert firmware_version == target_version_firmware


def test_failing_image_recovery(console):
    """
    Test recovery from a firmware image that boots successfully but then crashes.
    Expected behaviour:
     - Flash a local FW image that is known to boot but then crash after ~10s.
     - Verify that the FW boots successfully by reading its version.
     - Wait for the FW to crash.
     - Confirm the crash by verifying that the console becomes unresponsive.
     - Initiate recovery via a powercycle to get back to the bootloader.
     - Enter BSL mode and flash a known-good, stable FW version.
     - Verify that the device successfully boots and runs the new, stable FW.
    """
    # Since the all images stored in Google Cloud are in working order
    # we will use a local copy for the time being
    # TODO(b/438110087) Change this test to use a current copy
    failing_image_path = os.getcwd() + "/../images/1.233.0-acc33f7-failing/firmware.txt"

    console.update_firmware_local(failing_image_path)
    time.sleep(DELAY_VERY_SMALL)

    target_version_suffix = os.getenv("TARGET_FW_VERSION")[1:]
    target_version_firmware = "1" + target_version_suffix
    target_version_bootloader = "2" + target_version_suffix

    bootloader_version = console.run_firmware_command("version", bootloader=True)
    # Bootloader prints version as "Bootloader version 2.x"
    # so check if version is substring not equality
    assert target_version_bootloader in bootloader_version

    console.run_firmware_command("boot", bootloader=True)
    time.sleep(DELAY_VERY_SMALL)

    # This only succeeds if firmware is running
    assert console.get_version() is not None

    # Failing image is setup to fail after about 10s
    time.sleep(BOOTLOADER_TIMEOUT_EXTRA)

    # Confirm console is unresponsive
    with pytest.raises(DolosConsoleNoEchoError):
        console.get_version()

    ### START RECOVERY ###
    console.close()
    commands.dolos_powercycle()
    console.open()
    time.sleep(DELAY_SMALL)

    console.run_firmware_command("bsl", bootloader=True)
    console.update_firmware(target_version_firmware, bsl_mode=True)
    # Let bootloader timeout
    time.sleep(BOOTLOADER_TIMEOUT_EXTRA)

    post_recovery_version = console.get_version()
    assert post_recovery_version == target_version_firmware
