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

from doloscmd.doloscmd import main
from doloscmd.proto import doloscmd_pb2
from doloscmd.tests.fixtures.mock_dolos_console import DolosStatus
import pytest


class TestDolosCmdGetStatus:
    def test_get_status_no_serial_flag(
        self, monkeypatch, mock_host_with_one_dolos, capfd
    ):
        mock_host_with_one_dolos()

        with monkeypatch.context() as patch:
            patch.setattr(sys, "argv", ["dolos-cmd", "get-status"])

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

    def test_get_status_ok_dolos(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.serial_number = "DOLOSV1-C-2403140001"
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                ["dolos-cmd", "get-status", "--serial", "DOLOSV1-C-2403140001"],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_OK"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_ok_uart(self, monkeypatch, mock_host_with_one_dolos, capfd):
        mock_host_with_one_dolos()

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_OK"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_smbus_fail(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.smbus = "Not detected"
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_SMBUS_COMM_NOT_DETECTED"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_bms_fail(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.bms_state = "BMS_STATE_POWER_OUTPUT_OFF"
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_BMS_STATE_INVALID"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_efuse_fail(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.efuse = "Not good"
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_OUTPUT_POWER_FAILED"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_eeprom_fail(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.eeprom = "EEPROM Read Error."
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_EEPROM_FAILURE"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_charger_fail(
        self, monkeypatch, mock_host_with_one_dolos, capfd
    ):
        status = DolosStatus()
        status.charger = "Not Detected"
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_NO_POWER_SUPPLIED"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_not_found(self, monkeypatch, mock_host_with_one_dolos, capfd):
        host = mock_host_with_one_dolos()
        host.clear_usb_devices()
        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_NOT_PRESENT"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_no_comms(self, monkeypatch, mock_host_with_one_dolos, capfd):
        host = mock_host_with_one_dolos()
        host.raise_on_open()
        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                [
                    "dolos-cmd",
                    "get-status",
                    "--uartname",
                    "usb-FTDI_FT232R_USB_UART_B001I258-if00-port0",
                ],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_NOT_PRESENT"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_bad_crc(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.serial_number = "DOLOSV1-C-2403140001"
        status.eeprom = "CRC Failure."
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                ["dolos-cmd", "get-status", "--serial", "DOLOSV1-C-2403140001"],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_EEPROM_FAILURE"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code

    def test_get_status_bad_eeprom(self, monkeypatch, mock_host_with_one_dolos, capfd):
        status = DolosStatus()
        status.serial_number = "No cable - can't read serial."
        status.eeprom = "EEPROM Read Error."
        mock_host_with_one_dolos(status=status)

        with monkeypatch.context() as patch:
            patch.setattr(
                sys,
                "argv",
                ["dolos-cmd", "get-status", "--serial", "DOLOSV1-C-2403140001"],
            )
            with pytest.raises(SystemExit) as pytest_wrapped_e:
                main()
            out, unused_err = capfd.readouterr()
            assert '{\n  "status": "DOLOS_NOT_PRESENT"\n}\n' == out
            assert SystemExit == pytest_wrapped_e.type
            assert doloscmd_pb2.ERROR_CODE.OK == pytest_wrapped_e.value.code
