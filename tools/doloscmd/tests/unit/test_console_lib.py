# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=redefined-outer-name
# pylint: disable=unused-argument
# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring
# When pylint supports proto better remove
# pylint: disable=no-member

from unittest.mock import call
from unittest.mock import MagicMock

from doloscmd.console_lib import DolosConsole
from doloscmd.console_lib import DolosConsoleError
from doloscmd.proto import doloscmd_pb2
import pytest


class ConsoleLibTestBase:
    @pytest.fixture
    def mock_serial_port(self, mocker):
        """Mocks the serial.Serial constructor and its instance."""
        mock_serial_instance = MagicMock()
        # Ensure the mock serial instance can handle attribute assignments like 'timeout'
        # and calls to methods like 'read_until', 'write', 'flush', etc.
        # MagicMock handles these by default, creating them on access if they don't exist.
        mock_serial_instance.port = "dummy_mock_port"
        return mocker.patch(
            "doloscmd.console_lib.serial.Serial",
            return_value=mock_serial_instance,
        )

    @pytest.fixture
    def console_instance_genai(self, mock_serial_port, mock_console_init_run_cmd_genai):
        """
               Provides a DolosConsole instance with __init__ dependencies mocked.
               The mock_serial_port and mock_console_init_run_cmd_genai
        fixtures ensure
               that the DolosConsole can be instantiated without real hardware interaction.
        """
        console = DolosConsole("dummy_uartname_for_test")
        return console

    @pytest.fixture
    def mock_console_init_run_cmd_genai(self, mocker):
        """
        Mocks the DolosConsole.run_firmware_command specifically for the
        call made during DolosConsole.__init__.
        """
        return mocker.patch.object(
            DolosConsole,
            "run_firmware_command",
            return_value="Dolos version 1.0\ndolos:~> ",
        )

    @pytest.fixture
    def console_instance_genai(self, mock_serial_port, mock_console_init_run_cmd_genai):
        """
               Provides a DolosConsole instance with __init__ dependencies mocked.
               The mock_serial_port and mock_console_init_run_cmd_genai
        fixtures ensure
               that the DolosConsole can be instantiated without real hardware interaction.
        """
        console = DolosConsole("dummy_uartname_for_test")
        return console


class TestDolosConsoleRepairNoPowerSuppliedGenAI(ConsoleLibTestBase):

    def test_repair_genai(self, mocker, console_instance_genai):
        """
        Tests that repair commands are run if status is DOLOS_OUTPUT_POWER_FAILED.
        """
        mock_run_cmd = mocker.patch.object(
            console_instance_genai, "run_firmware_command"
        )
        mock_sleep = mocker.patch("doloscmd.console_lib.time.sleep")

        console_instance_genai.repair_output_power_failed()

        expected_calls = [
            call("sys_pres on", timeout=5),
            call("sys_pres disable", timeout=5),
        ]
        mock_run_cmd.assert_has_calls(expected_calls)
        assert mock_run_cmd.call_count == 2
        mock_sleep.assert_has_calls([call(10), call(5)])

    def test_repair_propagates_error_from_first_command_genai(
        self, mocker, console_instance_genai
    ):
        """Tests that an error from run_firmware_command is propagated."""
        mocker.patch.object(
            console_instance_genai,
            "determine_status",
            return_value=doloscmd_pb2.DOLOS_STATUS.DOLOS_OUTPUT_POWER_FAILED,
        )
        mocker.patch.object(
            console_instance_genai, "get_status", return_value={"E-Fuse Power": "Bad"}
        )
        mock_run_cmd = mocker.patch.object(
            console_instance_genai,
            "run_firmware_command",
            side_effect=DolosConsoleError("Command execution failed"),
        )
        mock_sleep = mocker.patch("doloscmd.console_lib.time.sleep")

        with pytest.raises(DolosConsoleError, match="Command execution failed"):
            console_instance_genai.repair_output_power_failed()

        mock_run_cmd.assert_called_once_with("sys_pres on", timeout=5)
        mock_sleep.assert_not_called()


class TestDolosConsoleRepairGenAI(ConsoleLibTestBase):

    def test_repair_basic_flow_status_ok(self, mocker, console_instance_genai):
        """
        Tests the basic repair flow when initial status is OK.
        Ensures reset is called, sleep occurs, status is checked,
        and sub-repair is not called.
        """
        mock_run_cmd = mocker.patch.object(
            console_instance_genai, "run_firmware_command", return_value="reset output"
        )
        mock_sleep = mocker.patch("doloscmd.console_lib.time.sleep")
        mock_get_status = mocker.patch.object(
            console_instance_genai, "get_status", return_value={"E-Fuse Power": "Good"}
        )
        mock_determine_status = mocker.patch.object(
            console_instance_genai,
            "determine_status",
            return_value=doloscmd_pb2.DOLOS_STATUS.DOLOS_OK,
        )
        mock_repair_output_failed = mocker.patch.object(
            console_instance_genai, "repair_output_power_failed"
        )

        result = console_instance_genai.repair()

        assert result == "reset output"
        mock_run_cmd.assert_called_once_with("reset", timeout=5)
        mock_sleep.assert_called_once_with(3)
        mock_get_status.assert_called_once()
        mock_determine_status.assert_called_once_with({"E-Fuse Power": "Good"})
        mock_repair_output_failed.assert_not_called()

    def test_repair_basic_flow_status_ok_with_status_arg_genai(
        self, mocker, console_instance_genai
    ):
        """
        Tests repair flow when status=OK is passed as an argument.
        Ensures status checks are skipped.
        """
        mock_run_cmd = mocker.patch.object(
            console_instance_genai, "run_firmware_command", return_value="reset output"
        )
        mock_sleep = mocker.patch("doloscmd.console_lib.time.sleep")
        mock_get_status = mocker.patch.object(console_instance_genai, "get_status")
        mock_determine_status = mocker.patch.object(
            console_instance_genai, "determine_status"
        )
        mock_repair_output_failed = mocker.patch.object(
            console_instance_genai, "repair_output_power_failed"
        )

        console_instance_genai.repair(status=doloscmd_pb2.DOLOS_STATUS.DOLOS_OK)

        mock_run_cmd.assert_called_once_with("reset", timeout=5)
        mock_sleep.assert_called_once_with(3)
        mock_get_status.assert_not_called()
        mock_determine_status.assert_not_called()
        mock_repair_output_failed.assert_not_called()

    def test_repair_calls_sub_repair_if_output_power_failed_genai(
        self, mocker, console_instance_genai
    ):
        """
        Tests that repair_output_power_failed is called if status check
        reveals DOLOS_OUTPUT_POWER_FAILED.
        """
        mocker.patch.object(
            console_instance_genai, "run_firmware_command", return_value="reset output"
        )
        mocker.patch("doloscmd.console_lib.time.sleep")
        mocker.patch.object(
            console_instance_genai, "get_status", return_value={"E-Fuse Power": "Bad"}
        )
        mocker.patch.object(
            console_instance_genai,
            "determine_status",
            return_value=doloscmd_pb2.DOLOS_STATUS.DOLOS_OUTPUT_POWER_FAILED,
        )
        mock_repair_output_failed = mocker.patch.object(
            console_instance_genai, "repair_output_power_failed"
        )

        console_instance_genai.repair()

        mock_repair_output_failed.assert_called_once()

    def test_repair_calls_sub_repair_if_output_power_failed_with_status_arg_genai(
        self, mocker, console_instance_genai
    ):
        """
        Tests that repair_output_power_failed is called if status=DOLOS_OUTPUT_POWER_FAILED
        is passed as an argument.
        """
        mocker.patch.object(
            console_instance_genai, "run_firmware_command", return_value="reset output"
        )
        mocker.patch("doloscmd.console_lib.time.sleep")
        mock_repair_output_failed = mocker.patch.object(
            console_instance_genai, "repair_output_power_failed"
        )

        console_instance_genai.repair(
            status=doloscmd_pb2.DOLOS_STATUS.DOLOS_OUTPUT_POWER_FAILED
        )

        mock_repair_output_failed.assert_called_once()

    def test_repair_propagates_error_from_reset_command_genai(
        self, mocker, console_instance_genai
    ):
        """Tests that an error from the 'reset' command is propagated."""
        mock_run_cmd = mocker.patch.object(
            console_instance_genai,
            "run_firmware_command",
            side_effect=DolosConsoleError("Reset failed"),
        )
        mock_sleep = mocker.patch("doloscmd.console_lib.time.sleep")

        with pytest.raises(DolosConsoleError, match="Reset failed"):
            console_instance_genai.repair()

        mock_run_cmd.assert_called_once_with("reset", timeout=5)
        mock_sleep.assert_not_called()

    def test_repair_propagates_error_from_get_status_genai(
        self, mocker, console_instance_genai
    ):
        """Tests that an error from get_status is propagated."""
        mocker.patch.object(console_instance_genai, "run_firmware_command")
        mocker.patch("doloscmd.console_lib.time.sleep")
        mocker.patch.object(
            console_instance_genai,
            "get_status",
            side_effect=DolosConsoleError("GetStatus failed"),
        )

        with pytest.raises(DolosConsoleError, match="GetStatus failed"):
            console_instance_genai.repair()

    def test_repair_propagates_error_from_repair_output_power_failed_call_genai(
        self, mocker, console_instance_genai
    ):
        """Tests error propagation from repair_output_power_failed."""
        mocker.patch.object(console_instance_genai, "run_firmware_command")
        mocker.patch.object(console_instance_genai, "get_status")
        mocker.patch.object(
            console_instance_genai,
            "determine_status",
            return_value=doloscmd_pb2.DOLOS_STATUS.DOLOS_OUTPUT_POWER_FAILED,
        )
        mocker.patch.object(
            console_instance_genai,
            "repair_output_power_failed",
            side_effect=DolosConsoleError("Sub-repair failed"),
        )

        with pytest.raises(DolosConsoleError, match="Sub-repair failed"):
            console_instance_genai.repair()
