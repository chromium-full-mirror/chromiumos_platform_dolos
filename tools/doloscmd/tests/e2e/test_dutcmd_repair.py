# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=redefined-outer-name
# pylint: disable=unused-argument
# pylint: disable=import-error
# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring
# When pylint supports proto better remove
# pylint: disable=no-member

import sys
import unittest

from doloscmd.doloscmd import main
from doloscmd.proto import doloscmd_pb2
from doloscmd.tests.fixtures.mock_dolos_console import DolosStatus
import pytest


class TestDolosCmdRepair:
    def test_repair_no_serial_flag(self, monkeypatch, mock_host_with_one_dolos, capfd):
        mock_host_with_one_dolos()

        with monkeypatch.context() as patch:
            patch.setattr(sys, "argv", ["dolos-cmd", "repair"])

            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert (
                '{\n  "response": {\n'
                '    "msg": "Must specify the dolos serial number or the UART serial number.",\n'
                '    "code": "WRONG_ARGS"\n'
                "  }\n}\n"
            ) == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.WRONG_ARGS == pytest_wrapped_e.value.code

    def test_repair_dolos(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.serial_number = "DOLOSV1-C-2403140001"
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                ["dolos-cmd", "repair", "--serial", "DOLOSV1-C-2403140001"],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert out.strip() == "{}"
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_repair_uart(self, monkeypatch, mock_host_with_one_dolos, capfd):
        mock_host_with_one_dolos()

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "repair",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert out.strip() == "{}"
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_repair_efuse_fail(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.efuse = "Not good"
        host = mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "repair",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert out.strip() == "{}"
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code
            unittest.TestCase().assertCountEqual(
                host.usb_devices[
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0"
                ].write_history,
                [
                    "",
                    "\n",
                    "status",
                    "\n",
                    "sys_pres on",
                    "\n",
                    "sys_pres disable",
                    "\n",
                    "reset",
                    "\n",
                ],
            )
