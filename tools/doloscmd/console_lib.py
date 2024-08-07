# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# When pylint supports proto better remove
# pylint: disable=no-member

"""Code that abstracts the implementation of the Dolos serial console from users."""

import os
import pathlib
import re
import stat
import subprocess
import tempfile
import time

from doloscmd.proto import doloscmd_pb2
from doloscmd.utils import debug_print
import requests
import serial


FIRMWARE_URL = (
    "https://storage.googleapis.com/dolos-firmware/%(firmware_version)s/zephyr.txt"
)
SERIAL_PATH = "/dev/serial/by-id/"
DOLOS_FTDI_GLOB = "usb-FTDI_FT232R_USB_UART_*"

CONSOLE_PROMPT = "\x1b[m"

CONSOLE_CLEAR_CHR = chr(8)
CONSOLE_CLEAR = CONSOLE_CLEAR_CHR * 20
CONSOLE_ENTER = "\r"
CONSOLE_COMMAND_TEMPLATE = (
    f"{CONSOLE_CLEAR} {CONSOLE_ENTER} %(command)s {CONSOLE_ENTER}"
)


class DolosConsoleException(Exception):
    """General exception thrown when a error occurs dealing with the console."""


class DolosConsole:
    """Wrap the console implementation to a more user friendly interface."""

    def run_firmware_command(self, console, command):
        """Issue a command to the UART console and read back the result.

        Args:
            console (serial.Serial): object that wraps an open connection to the UART.
            command (string): command to issue.

        Returns:
            list[string]: The output read from the console.
        """
        console.reset_output_buffer()
        console.reset_input_buffer()
        # Sometimes there is junk output from the console from prior connections.
        # ignore everything before we issue a command.
        while console.read():
            pass

        encapulated_command = CONSOLE_COMMAND_TEMPLATE % {"command": command}
        console.write(encapulated_command.encode("utf-8"))
        console.flush()
        # Reading the console too quickly will result in incomplete reads.
        # 0.2 seconds was established by experimentation.
        time.sleep(0.2)
        try:
            results = console.read_until(CONSOLE_PROMPT).split(b" ")
        except (serial.SerialException, OSError) as error:
            debug_print(str(error))
            raise DolosConsoleException(f"Serial read fail {error}") from error

        results = [result.decode("utf-8") for result in results]

        debug_print(f"Serial read: {results}")

        return results

    def run_command_on_dolos(self, uartname, func):
        """Open a serial connection to a UART and execute the given function.

        This function deals with the details of the UART name to device mapping,
        serial connection and closing.

        Args:
            uartname (string): The name of the UART device to connect to.
            func (function(serial.Serial)): A function that takes an open serial connection

        Raises:
            DolosConsoleException: Connection or other serial issue is detected.

        Returns:
            _type_: _description_
        """
        if not uartname:
            raise DolosConsoleException("No dolos device found.")
        result = None
        port = SERIAL_PATH + uartname
        try:
            with serial.Serial(
                port=str(port), baudrate=115200, timeout=0.5, exclusive=True
            ) as console:
                result = func(console)
        except serial.SerialException as error:
            raise DolosConsoleException(
                f"Unable to open serial device {port}.\rReason: {str(error)}",
            ) from error
        return result

    def run_command_on_all_dolos(self, func):
        """Execute a console command on every Dolos connected to the host.

        Find all the dolos devices that match the UART pattern, and call
        the given function on each dolos console.

        Args:
            func(function): A function that will interact with the UART console.

        Returns:
            dict[string: function return]: A dictionary with the uartname as the key
            and the return value of the function as the value.
        """
        results = {}
        for port in self.get_dolos_uart_names():
            result = self.run_command_on_dolos(port, func)
            results[str(port)] = result
        return results

    def get_version(self, console):
        """Call version command on UART and parse results.

        Args:
            console (serial.Serial): object that wraps an open connection to the UART.

        Returns:
            string: version of the Dolos firmware.
        """
        result = self.run_firmware_command(console, "version")
        version = " ".join(result).split()[5]
        return version

    def repair(self, console):
        """Call reset command on the UART.

        This function was specifically not called reset as in the future more console
        commands may be added to the repair.

        Args:
            console (serial.Serial): object that wraps an open connection to the UART.

        Returns:
            bytes: Any output read from the console before the connection closes.
        """
        return self.run_firmware_command(console, "reset")

    def dolos_status_output_to_dict(self, lines):
        """Convert the raw bytes from the status console command to a dictionary of strings.

        The status command outputs several likes of information, parse that and put it in
        and indexable format.

        See the template in tests/fixtures/mock_dolos_console.py for example of input
        format.

        Args:
            lines (bytes): The raw output from the console command status.


        Returns:
            dict{string: string} : Mapping of status keys to values.
        """
        status_map = {}
        for line in lines[2:-1]:
            sections = line.split(":")
            if len(sections) != 2:
                continue
            key = sections[0].strip()
            value = sections[1].strip()
            status_map[key] = value
        return status_map

    def get_status(self, console):
        """Call status command on the UART.

        Args:
            console (serial.Serial): object that wraps an open connection to the UART.

        Returns:
            dict{string: string} : Mapping of status keys to values.
        """
        result = self.run_firmware_command(console, "status")
        processed_results = " ".join(result).splitlines()
        return self.dolos_status_output_to_dict(processed_results)

    def fix_broken_serial(self, dolos_serial_number):
        """Fix serial number format incorrectly written in manufacturing.

        The Dolos serial numbers are supposed to be :
        DOLOSV1-C-1520240001

        but some are programmed like:
        dolos-V1-2415-0001

        Identify a serial number in the incorrect format and correct it.

        Args:
            dolos_serial_number (string): Dolos serial number.

        Returns:
            string: The original version string passed or corrected string if it
            matches the known bad format.
        """
        # Some firmware give the serial number as dolos-V1-2415-0001 this is incorrect
        # needs to be DOLOSV1-C-1520240001 - for now just convert it.
        match = re.match(
            r"^dolos-V1-([0-9]{2})([0-9]{2})-([0-9A-F]{4})$", dolos_serial_number
        )
        if match:
            return f"DOLOSV1-C-{match.group(2)}20{match.group(1)}{int(match.group(3), 16):04d}"

        return dolos_serial_number

    def get_serial(self, console):
        """Get the serial number from the status output on the UART.

        Args:
            console (serial.Serial): object that wraps an open connection to the UART.

        Returns:
            string: Dolos serial number.
        """
        status_dict = self.get_status(console)
        debug_print(status_dict)
        dolos_serial_number = self.fix_broken_serial(
            status_dict.get("Serial number", "No Serial Number")
        )
        return dolos_serial_number

    def get_dolos_uart_names(self):
        """Find all the UART devices attached to the host that are assumed to be Dolos.

        At this time the UART glob is very generic but we know that this only runs
        on labstation and there are no other UART names that match that are not Dolos.

        Returns:
            list[string]: UART names, no including the file path to the device.
        """
        ports = pathlib.Path(SERIAL_PATH).glob(DOLOS_FTDI_GLOB)
        ports = [str(port) for port in ports]
        ports = [port[len(SERIAL_PATH) :] for port in ports]
        return ports

    def find_dolos_serial(self, dolos_serial):
        """Given a Dolos serial number find the correct UART that has that serial.

        The only way to map Dolos serial number to UART is to open a connection to each
        device, parse the status and look to see if the serial number matches.

        TODO(haddowk)  This is slow, the fleet is planning to cache the UART name but
        look at doing the query's in parallel.

        Args:
            dolos_serial (string): Dolos serial number to find.

        Returns:
            string: The name of the UART found or None if not found.
        """
        serial_nos = self.run_command_on_all_dolos(self.get_serial)
        debug_print(f"serial_nos: {serial_nos}")
        for usb_device, serial_no in serial_nos.items():
            debug_print(f"looking for {dolos_serial}, found {serial_no}")
            if dolos_serial == serial_no:
                return usb_device
        debug_print(f"No dolos found for serial {dolos_serial}.")
        return None

    def update_firmware(self, uartname, firmware_version):
        """Update the Dolos firmware to a different version.

        Both the updater and the firmware are posted on a public Google Cloud bucket, fetch
        both into a temporary directory and run the updater.

        Both the updater and firmware are very small so cost of download is not high enough to
        warrant a complex caching strategy.

        Args:
            uartname (string): The name of the Dolos UART that maps to the device to update.
            firmware_version (string): version of the firmware to update to.
        """
        with tempfile.TemporaryDirectory(prefix="/usr/local/tmp/") as temp_directory:
            firmware_updater_filename = f"{temp_directory}/fw-updater"
            req = requests.get(
                "https://storage.googleapis.com/dolos-firmware/fw-updater",
                stream=True,
                timeout=180,
            )
            with open(firmware_updater_filename, "wb") as download_file:
                for chunk in req.iter_content(chunk_size=128):
                    download_file.write(chunk)

            st = os.stat(firmware_updater_filename)
            os.chmod(firmware_updater_filename, st.st_mode | stat.S_IEXEC)

            dolo_hex_firmware_filename = f"{temp_directory}/zephyr.txt"
            req = requests.get(
                FIRMWARE_URL % {"firmware_version": firmware_version},
                stream=True,
                timeout=180,
            )
            with open(dolo_hex_firmware_filename, "wb") as download_file:
                for chunk in req.iter_content(chunk_size=128):
                    download_file.write(chunk)
            try:
                command = [
                    f"{temp_directory}/fw-updater",
                    "--uart",
                    SERIAL_PATH + str(uartname),
                    f"{temp_directory}/zephyr.txt",
                ]
                process = subprocess.run(
                    command,
                    check=False,
                    capture_output=True,
                )
            except subprocess.CalledProcessError as error:
                debug_print(str(error))
                raise DolosConsoleException(str(error)) from error
            debug_print(
                f"Firmware update {' '.join(command)}  "
                f"stdout: {repr(process.stdout)} "
                f"stderr: {repr(process.stderr)}"
            )

    def get_uartname(self, args):
        """Determine the actual UART name to act upon.

        The user can specify the UART name or the Dolos serial on the command line,
        determine the actual UART name and check that it is actually connected to
        this host.

        Args:
            args (Namespace): Parsed command line arguments.

        Raises:
            DolosConsoleException: Raised if the specified UART is not found.

        Returns:
            string: the name of a UART connected to the device as specified by user.
        """
        uartname = args.uartname
        if not uartname:
            uartname = self.find_dolos_serial(args.serial)

        if uartname:
            ports = self.get_dolos_uart_names()
            debug_print(f"ports: {ports}")
            if uartname in ports:
                return uartname
            raise DolosConsoleException(
                f"Device {uartname} not found on host.",
            )
        return None

    def determine_status(self, status_dict):
        """The external status of the device is abstracted from the internal status.

        This maps status command values to the correct external error status.

        There are other error status's that are not dependent on the console status output
        they are implemented elsewhere.

        Args:
            status_dict (dict{string: string}): Mapping of status console keys to values.

        Returns:
            DOLOS_STATUS: The calculated status of the Dolos.
        """
        # This is the implementation of go/cros-dolos-status

        if status_dict.get("Charger", "") != "Detected":
            return doloscmd_pb2.DOLOS_STATUS.DOLOS_NO_POWER_SUPPLIED
        if status_dict.get("E-Fuse Power", "") != "Good":
            return doloscmd_pb2.DOLOS_STATUS.DOLOS_OUTPUT_POWER_FAILED
        if status_dict.get("BMS current state", "") != "BMS_STATE_POWER_OUTPUT_ON":
            return doloscmd_pb2.DOLOS_STATUS.DOLOS_BMS_STATE_INVALID
        if status_dict.get("SMBus communication", "") != "Detected":
            return doloscmd_pb2.DOLOS_STATUS.DOLOS_SMBUS_COMM_NOT_DETECTED
        if status_dict.get("EEPROM", "") != "Successful":
            return doloscmd_pb2.DOLOS_STATUS.DOLOS_EEPROM_FAILURE
        return doloscmd_pb2.DOLOS_STATUS.DOLOS_OK
