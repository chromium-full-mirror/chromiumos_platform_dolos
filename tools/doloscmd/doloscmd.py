#!/usr/bin/env python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This command is designed as an abstraction between the test infrastructure and the
# Dolos serial console, to provide a strong contract between the two and allow changes
# in the console to not impact users.

# When pylint supports proto better remove
# pylint: disable=no-member

"""Provide a command line interface for to the Dolos UART console."""

import argparse
import logging
import pathlib
import sys

from doloscmd import console_lib
from doloscmd.proto import doloscmd_pb2
from google.protobuf.json_format import MessageToJson


def _exit_if_no_serial(args, response_class):
    """Check to see if the uart name or the dolos serial is on the command line.

    Args:
        args (object): parsed command line arguments.
        response_class (_type_): _description_
    """
    if not args.serial and not args.uartname:
        _do_exit(
            response_class(
                response=doloscmd_pb2.Response(
                    msg=(
                        "Must specify the dolos serial number or the UART "
                        "serial number."
                    ),
                    code=doloscmd_pb2.ERROR_CODE.WRONG_ARGS,
                ),
            )
        )


def parse_args(args):
    """Parse the command line arguments.
    Args:
        args (list[str]): The list of command line arguments passed to the script.

    Returns:
       Namespace: parsed command line arguments.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "command",
        choices=[
            "find-uartname",
            "update-firmware",
            "get-status",
            "repair",
            "version",
            "program-cable",
        ],
    )
    parser.add_argument("--serial")
    parser.add_argument("--uartname")
    parser.add_argument("--firmware_version")
    parser.add_argument(
        "--hwid", help="Optional model name or full HWID when programming cable"
    )
    parser.add_argument("--file", help="Optional file path", type=pathlib.Path)
    parser.add_argument(
        "--new_serial", help="Optional replacement serial when programming cable"
    )
    parser.add_argument("--bsl_mode", action="store_true")

    return parser.parse_args(args)


def _do_exit(message):
    """Exit the program with the appropriate code and message to stdout.

    Args:
        message (Object): A dolos command method response protobuf object.
    """
    print(
        MessageToJson(
            message,
            preserving_proto_field_name=True,
        )
    )
    sys.exit(message.response.code)


def _update_firmware(args):
    """Update dolos to a new firmware version.

    Args:
        args (Namespace): parsed command line arguments.
    """
    _exit_if_no_serial(args, doloscmd_pb2.FirmwareUpdateResponse)

    if not args.firmware_version:
        _do_exit(
            doloscmd_pb2.FirmwareUpdateResponse(
                response=doloscmd_pb2.Response(
                    msg="No firmware_version specified.",
                    code=doloscmd_pb2.ERROR_CODE.WRONG_ARGS,
                )
            )
        )

    console = None
    try:
        console = console_lib.DolosConsole.find_dolos_by_args(args)
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.FirmwareUpdateResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.NOT_FOUND
                )
            )
        )
        return
    console.update_firmware(args.firmware_version, args.bsl_mode)
    _do_exit(doloscmd_pb2.GetVersionResponse())


def version(args):
    """Get the current dolos firmware version.

    Args:
        args (Namespace): parsed command line arguments.
    """
    _exit_if_no_serial(args, doloscmd_pb2.GetVersionResponse)

    console = None
    try:
        console = console_lib.DolosConsole.find_dolos_by_args(args)
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.GetVersionResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.NOT_FOUND
                )
            )
        )
        return

    logging.debug("version uartname %r", console.uartname)
    try:
        fw_version = console.get_version()
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.GetVersionResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.INTERNAL
                )
            )
        )
    _do_exit(doloscmd_pb2.GetVersionResponse(version=fw_version))


def repair(args):
    """Repair the dolos.

    Args:
        args (Namespace): parsed command line arguments.
    """
    _exit_if_no_serial(args, doloscmd_pb2.GetRepairResponse)

    console = None
    try:
        console = console_lib.DolosConsole.find_dolos_by_args(args)
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.GetRepairResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.NOT_FOUND
                )
            )
        )
        return
    try:
        console.repair()
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.GetRepairResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.INTERNAL
                )
            )
        )
    _do_exit(doloscmd_pb2.GetRepairResponse())


def get_status(args):
    """Report the current status of the Dolos.

    Args:
        args (Namespace): parsed command line arguments.
    """
    _exit_if_no_serial(args, doloscmd_pb2.GetStatusResponse)

    console = None
    try:
        console = console_lib.DolosConsole.find_dolos_by_args(args)
    except console_lib.DolosConsoleError:
        _do_exit(
            doloscmd_pb2.GetStatusResponse(
                status=doloscmd_pb2.DOLOS_STATUS.DOLOS_NOT_PRESENT
            )
        )
        return
    try:
        status = console.determine_status(console.get_status())

        _do_exit(doloscmd_pb2.GetStatusResponse(status=status))
    except console_lib.DolosConsoleError:
        _do_exit(
            doloscmd_pb2.GetStatusResponse(
                status=doloscmd_pb2.DOLOS_STATUS.DOLOS_NO_COMMUNICATION
            )
        )


def find_uartname(args):
    """Given a Dolos serial number find what UART name maps to it.

    Args:
        args (Namespace): parsed command line arguments
    """
    if not args.serial:
        _do_exit(
            doloscmd_pb2.FindUartNameResponse(
                response=doloscmd_pb2.Response(
                    msg="Must specify the dolos serial number to find.",
                    code=doloscmd_pb2.ERROR_CODE.WRONG_ARGS,
                )
            )
        )
        return
    console = None
    try:
        console = console_lib.DolosConsole.find_dolos_by_args(args)
    except console_lib.DolosConsoleError:
        _do_exit(
            doloscmd_pb2.FindUartNameResponse(
                response=doloscmd_pb2.Response(
                    msg="Failed to find dolos.",
                    code=doloscmd_pb2.ERROR_CODE.NOT_FOUND,
                )
            )
        )
        return

    _do_exit(
        doloscmd_pb2.FindUartNameResponse(
            uartname=console.uartname,
        )
    )


def program_cable(args):
    """Update dolos to a new firmware version.

    Args:
        args (Namespace): parsed command line arguments.
    """
    _exit_if_no_serial(args, doloscmd_pb2.FirmwareUpdateResponse)
    console = None
    try:
        console = console_lib.DolosConsole.find_dolos_by_args(args)
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.FirmwareUpdateResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.NOT_FOUND
                )
            )
        )
        return
    try:
        console.program_cable(args.hwid, args.file, args.new_serial)
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.FirmwareUpdateResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.NOT_FOUND
                )
            )
        )
        return
    _do_exit(doloscmd_pb2.GetVersionResponse())


def main():
    """Parse the args and call the correct function for command specified."""
    args = parse_args(sys.argv[1:])
    logging.basicConfig(level=logging.DEBUG)
    try:
        if args.command == "find-uartname":
            find_uartname(args)
        elif args.command == "update-firmware":
            _update_firmware(args)
        elif args.command == "get-status":
            get_status(args)
        elif args.command == "repair":
            repair(args)
        elif args.command == "version":
            version(args)
        elif args.command == "program-cable":
            program_cable(args)
        else:
            _do_exit(
                doloscmd_pb2.GenericResponse(
                    response=doloscmd_pb2.Response(
                        msg="Unexpected Method Called",
                        code=doloscmd_pb2.ERROR_CODE.UNEXPECTED,
                    )
                )
            )
    except console_lib.DolosConsoleError as error:
        _do_exit(
            doloscmd_pb2.GenericResponse(
                response=doloscmd_pb2.Response(
                    msg=str(error), code=doloscmd_pb2.ERROR_CODE.INTERNAL
                )
            )
        )


if __name__ == "__main__":
    main()
    sys.exit(0)
