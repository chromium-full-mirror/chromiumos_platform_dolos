# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# When pylint supports proto better remove
# pylint: disable=no-member

# Might warrant a refactor in the future
# pylint: disable=too-many-lines
# pylint: disable=too-many-public-methods

"""Code that abstracts the implementation of the Dolos serial console from users."""

from datetime import timedelta
import logging
from multiprocessing import Pool
import os
import pathlib
import re
import stat
import subprocess
import tempfile
import time

from doloscmd.config import Config
from doloscmd.eeprom_layout import EEPROMLayout
from doloscmd.error import DolosConsoleError
from doloscmd.error import DolosConsoleNoEchoError
from doloscmd.proto import doloscmd_pb2
import requests
import serial


FIRMWARE_URL = (
    "https://storage.googleapis.com/dolos-firmware/"
    "box_firmware/%(firmware_version)s/zephyr.txt"
)
BOOTLOADER_URL = (
    "https://storage.googleapis.com/dolos-firmware/"
    "box_firmware/bootloader/%(bootloader_version)s/boot.txt"
)
COMBINED_URL = (
    "https://storage.googleapis.com/dolos-firmware/"
    "box_firmware/%(combined_version)s/combined.txt"
)
# TODO(b/438110087): Change URLs when removing support for legacy

SERIAL_PATH = "/dev/serial/by-id/"
DOLOS_FTDI_GLOB = "usb-FTDI_FT232R_USB_UART_*"

TEMP_DIR = "/usr/local/tmp/"

CONSOLE_PROMPT = "dolos:~> \x1b[m"
BOOTLOADER_PROMPT = "boot:~> \x1b[m"

CONSOLE_CLEAR = chr(0x15)  # Ctrl-U metakey clears any text
CONSOLE_ENTER = "\n"
EEPROM_SIZE = 1024
EEPROM_WRITE_SIZE = 16


class DolosConsole:
    """Handles the connection with a Dolos Console and issuing commands to the
    Dolos device.

    Additional class methods allow the device discovery using serial numbers
    or generic arguments.
    """

    def __init__(self, uartname, open_timeout=1):
        """Creates a Dolos console from the uartname

        Opens a serial connection to a uartname. Connects to the device and
        issues a command to extract the serial number.

        Args:
            uartname (string): Uartname of the console
            open_timeout (number): Timeout in seconds to open the console

        Raises:
            DolosConsoleError: We are unable to open the serial port
        """
        self.uartname = uartname
        self.console = None
        self._serial = None

        # Open the console to verify it exists
        self.open(open_timeout)
        try:
            # Sometimes there is junk output in the console from prior
            # activity. Wait until the console is clear before issuing a
            # command.
            self.run_firmware_command("", timeout=1)
        except DolosConsoleError as err:
            logging.debug(err)

    def __del__(self):
        """Release resources."""
        self.close()

    def open(self, open_timeout=1):
        """Open a Dolos Serial console connection.

        Args:
            open_timeout (number): Timeout in seconds to open the console

        Raises:
            DolosConsoleError: Device failed to open.
        """
        path = SERIAL_PATH + self.uartname
        stop = time.monotonic() + open_timeout
        while True:
            try:
                self.console = serial.Serial(
                    port=path,
                    baudrate=115200,
                    exclusive=True,
                    timeout=10,
                    write_timeout=10,
                )
                break
            except (serial.SerialException, OSError) as err:
                # Check if we still have time
                if time.monotonic() < stop:
                    time.sleep(0.01)
                else:
                    # Timed out
                    raise DolosConsoleError(
                        f"Unable to open serial device {path}.\rReason: {str(err)}",
                    ) from err

    def close(self):
        """Closes the dolos console serial connection."""
        if self.console:
            self.console.close()
            self.console = None

    @classmethod
    def _get_all_ftdi_uart_names(cls):
        """Find all the UART devices attached to the host that maybe be Dolos.

        At this time the UART glob is very generic but we know that this only runs
        on labstation and there are no other UART names that match that are not Dolos.

        Returns:
            list[string]: UART names, no including the file path to the device.
        """
        ports = pathlib.Path(SERIAL_PATH).glob(DOLOS_FTDI_GLOB)
        ports = [str(port) for port in ports]
        ports = [port[len(SERIAL_PATH) :] for port in ports]
        return ports

    @classmethod
    def _safe_init(cls, uart, total_cnt):
        """Wraps the init call to catch exceptions.

        Handles the sequence to connect to a Dolos console, obtain it's serial,
        and close it. Catches and logs exceptions and returns a None value.
        This permits the multiprocessing map to load many serials at once.

        Args:
            uart (string): Uart port to connect to

        Returns:
            DolosConsole: Dolos console in the closed state
            None: Exception prevented connecting to the device
        """

        # It takes about 0.2 seconds to open a Dolos console load the serial.
        # If N dolos are present and each doloscmd needs to find a serial at the
        # same time N^2 calls will happen. These operations happen in parallel
        # so the open timeout is proportional to N.
        open_timeout = total_cnt * 0.5
        try:
            console = DolosConsole(uart, open_timeout)
            # The serial is set the first time it's accessed
            _ = console.serial
            console.close()
            return console
        except DolosConsoleError as err:
            logging.debug("Uart:%r, Error:%r", uart, err)
            return None

    @classmethod
    def get_all_dolos_consoles(cls):
        """Find all the Dolos consoles.

        Creates a list of all Dolos devices and their serials. The consoles
        are closed to reduce the time the resource is locked to a minimum.

        Returns:
            list[DolosConsole]: All known Dolos consoles in a closed state.
        """
        dolos = []
        uarts = cls._get_all_ftdi_uart_names()
        # No devices found
        if len(uarts) == 0:
            logging.debug("No Dolos consoles found")
            return []

        args = [(x, len(uarts)) for x in uarts]
        # Labstations have low CPU counts but we want the command to run
        # in parallel and _safe_init is I/O limited.
        with Pool(len(uarts)) as pool:
            dolos = pool.starmap(cls._safe_init, args)
        dolos = list(filter(None, dolos))

        logging.debug("%d Dolos consoles found", len(dolos))
        for d in dolos:
            logging.debug("Uart:%r, Serial:%r", d.uartname, d.serial)
        return dolos

    @classmethod
    def find_dolos_serial(cls, dolos_serial=None):
        """Given a Dolos serial number find the correct UART that has that serial.

        The only way to map Dolos serial number to UART is to open a connection to each
        device, parse the status and look to see if the serial number matches. This
        is also the only method to identify the Dolos from other FTDI devices.

        If other than 1 device matches, returns an exception.

        TODO(haddowk)  This is slow, the fleet is planning to cache the UART name but
        look at doing the query's in parallel.

        Args:
            dolos_serial (optional string): Optional Dolos serial number to find.
                            When None, this will connect to Dolos if only 1 device is
                            found.

        Returns:
            DolosConsole: DolosConsole instance

        Raises:
            DolosConsoleError: We did not find exactly 1 match
        """
        consoles = cls.get_all_dolos_consoles()

        # Find all consoles that match the criteria.
        if dolos_serial is not None:
            match = []
            for x in consoles:
                if x.serial == dolos_serial:
                    match.append(x)
            consoles = match
        if len(consoles) == 1:
            console = consoles[0]
            console.open()
            return console

        raise DolosConsoleError(f"Found {len(consoles)} dolos devices.")

    @classmethod
    def find_dolos_by_args(cls, args):
        """Finds the dolos console using an argparse Namespace.

        Args:
            args (argparse.Namespace): Args object with a uartname and serial field

        Returns:
            DolosConsole: Open Dolos console

        Raises:
            DolosConsoleError: Failed to find the device
        """
        if args.uartname:
            return DolosConsole(args.uartname)
        if args.serial:
            return cls.find_dolos_serial(args.serial)
        raise DolosConsoleError("Missing serial and uartname")

    def read_console(self, timeout=1, prompt=CONSOLE_PROMPT):
        """Reads the console and decodes the output

        Reads all text from the console until we see a console prompt or timeout
        whichever happens first.

        Args:
            timeout: Max time to read
            prompt: Prompt to look for (either firmware or bootloader)

        Returns:
            string: Dolos console output

        Raises:
            DolosConsoleError: Reading failed
        """

        if self.console is None:
            raise DolosConsoleError("Console closed")

        self.console.timeout = timeout

        try:
            results = self.console.read_until(prompt.encode("utf-8"))
        except (serial.SerialException, OSError) as err:
            logging.debug("Serial read fail %r", err)
            raise DolosConsoleError(f"Serial read fail {err}") from err

        results = results.decode("utf-8", "replace")
        results = re.sub(r"[\r\n]+", "\n", results)
        return results

    def _find_response_start(self, cmd, response):
        """Find out if the command has started and find the start of
        the response.

        We need to search the response for the command echo and detect it to
        confirm if it has started and when it starts. All text before the start
        of the response needs to be removed to avoid parsing errors.

        A complication exists because newlines are occasionally injected
        in the echos and we can't just keep the last one.

        Example:
            cmd = 'example cmd' and response = 'example cmd\n' is valid
            cmd = 'example cmd' and response = 'exam\nple cmd\n' is valid
            cmd = 'example cmd' and response = 'example cmd' is incomplete
            cmd = 'example cmd' and response = 'examp' is incomplete

        Returns:
            string: The start of the response after the command echo is complete.

        Raises:
            DolosConsoleError: Command echo not in the response indicating
                it may not have started.
        """
        cmd = cmd + "\n"

        # For the command to be run, there needs at least 1 newline. Every
        # cycle of this loop removes 1.
        while response.count("\n") > 0:
            # Check if the command can be detected
            if cmd in response:
                # Find the start of the response
                start = response.find(cmd) + len(cmd)
                return response[start:]
            # Trim the next newline.
            response = response.replace("\n", "", 1)
        raise DolosConsoleError(f"Device failed to echo command {cmd}")

    def _confirm_ready(self, response, prompt=CONSOLE_PROMPT):
        """
        Dolos is ready either when we detect a prompt (be it firmware or bootloader)
        or in the case of the bootloader and the non-returning commands
        when we detect '...' in the output

        Example:
            'Booting main image...' is non returning
        """
        return prompt in response or "..." in response

    def _run_and_confirm(
        self, cmd, echo_timeout=0.2, echo_retry=5, prompt=CONSOLE_PROMPT
    ):
        """Run a firmware command and confirm it's started.

        Experiments show Dolos does not always respond to commands after input.
        The full text will be present in the RX buffer. But something has
        prevented the shell from executing the command unless a new 'Enter'
        event is received.

        Repeating the whole command after a timeout creates problems as if the
        Dolos is busy, we may fill the RX buffer and be unable to recover.

        Args:
            cmd (string): command to issue.
            echo_timeout (number): Timeout before deciding command echo back failed
            echo_retry (integer): Number of attempts waiting for the command echo
            prompt (string): The prompt to look for

        Returns:
            string: The output read from the console.

        Raises:
            DolosConsoleError: Command failed to start
        """

        if self.console is None:
            raise DolosConsoleError("Console closed")

        self.console.reset_output_buffer()
        self.console.reset_input_buffer()
        # Clear any partial commands
        self.console.write(CONSOLE_CLEAR.encode("utf-8"))
        self.console.write(cmd.encode("utf-8"))
        response = ""
        logging.debug("Running command: %r", cmd)
        for _ in range(echo_retry):
            # Try pressing enter to start the command
            self.console.write(CONSOLE_ENTER.encode("utf-8"))
            self.console.flush()
            response += self.read_console(timeout=echo_timeout, prompt=prompt)
            try:
                return self._find_response_start(cmd, response)
            except DolosConsoleError:
                pass

        raise DolosConsoleNoEchoError(f"Device failed to echo command {cmd}")

    def run_firmware_command(self, cmd, timeout=1, echo_timeout=0.2, bootloader=False):
        """Issue a command to the UART console and read back the result.

        Args:
            cmd (string): command to issue.
            timeout (number): Timeout to await the response
            echo_timeout (number): Timeout to wait for echo
            bootloader (boolean): Flag to indicate that the command is
                                  meant for the bootloader

        Returns:
            string: The output read from the console.

        Raises:
            DolosConsoleError: Command failed to start or complete
        """

        # Select the prompt
        prompt = CONSOLE_PROMPT if not bootloader else BOOTLOADER_PROMPT

        # Run the command and read the response to confirm it's started
        response = self._run_and_confirm(cmd, echo_timeout=echo_timeout, prompt=prompt)

        # Sometimes we are dealing with a long command. Extra time is allocated
        if not self._confirm_ready(response, prompt=prompt):
            response += self.read_console(timeout=timeout, prompt=prompt)
        if not self._confirm_ready(response, prompt=prompt):
            raise DolosConsoleError(
                f"Command: {repr(cmd)} Timeout: {timeout} "
                + f" expired waiting for response {repr(response)}"
            )
        logging.debug("Command: %r Response: %r", cmd, response)
        return response

    def get_version(self, bootloader=False):
        """Call version command on UART and parse results.

        Args:
            bootloader (boolean): flag to query for bootloader version

        Returns:
            string: version of the Dolos firmware or bootloader.
        """
        result = self.run_firmware_command(
            "version" if not bootloader else "version_bootloader"
        )
        match = re.search(r" version (.*)\n", result)
        if match:
            return match.group(1)
        raise DolosConsoleError("Failed to find version")

    def repair_output_power_failed(self):
        """Attempts to repair Dolos by forcing sys_pres on and then off.

        Turn on/off sys_pres on the console to work around what we believe are
        occasional race conditions on boot up that cause DOLOS_OUTPUT_POWER_FAILED
        conditions (rarely)

        Raises:
            DolosConsoleError: If sending commands to the console fails.
        """
        self.run_firmware_command("sys_pres on", timeout=5)
        time.sleep(10)
        self.run_firmware_command("sys_pres disable", timeout=5)
        time.sleep(5)

    def repair(self, status=None):
        """Call reset command on the UART.

        This function was specifically not called reset as in the future more console
        commands may be added to the repair.

        Returns:
            string: Any output read from the console during the reset
        """
        # Extended timeout allows blocks us until the console restores
        try:
            self.run_firmware_command("reset", timeout=5)
        except DolosConsoleNoEchoError:
            # Running a command requires the command to echo back - this does not work
            # for reset.
            pass
        time.sleep(3)
        if not status:
            status = self.determine_status(self.get_status())
        if status == doloscmd_pb2.DOLOS_STATUS.DOLOS_OUTPUT_POWER_FAILED:
            self.repair_output_power_failed()

    def dolos_status_output_to_dict(self, lines):
        """Convert raw bytes from the status console command to dictionary of strings.

        The status command outputs several likes of information, parse that and put it
        in and indexable format.

        See the template in tests/fixtures/mock_dolos_console.py for example of input
        format.

        Args:
            lines (string): The raw output from the console command status.


        Returns:
            dict{string: string} : Mapping of status keys to values.
        """
        status_map = {}
        for line in lines[1:-1]:
            sections = line.split(":", 1)
            if len(sections) != 2:
                continue
            key = sections[0].strip()
            value = sections[1].strip()
            status_map[key] = value
        return status_map

    def get_status(self):
        """Call status command on the UART.

        This will update the serial number and return the status dictionary

        Returns:
            dict{string: string} : Mapping of status keys to values.
        """
        result = self.run_firmware_command("status")
        logging.debug("Status results: %r", result)
        processed_results = result.splitlines()
        logging.debug("Status processed_results: %r", processed_results)
        status_dict = self.dolos_status_output_to_dict(processed_results)
        logging.debug(status_dict)
        dolos_serial = self.fix_broken_serial(
            status_dict.get("Serial number", "No Serial Number")
        )
        status_dict["Serial number"] = dolos_serial
        self._serial = dolos_serial
        return status_dict

    def get_uptime(self):
        """Gets the uptime of the device.

        This method parses the 'Uptime' value from the status output and
        returns it as a timedelta object.

        Returns:
            datetime.timedelta: The uptime of the device, or None if the
                                uptime string cannot be parsed.
        """
        status = self.get_status()
        uptime_str = status.get("Uptime")

        if not uptime_str:
            logging.warning("Uptime information not found in status.")
            return None

        match = re.search(r"(\d+)\s+days.*(\d+):(\d+):(\d+)\s+seconds", uptime_str)
        if match:
            days = int(match.group(1))
            hours = int(match.group(2))
            minutes = int(match.group(3))
            seconds = int(match.group(4))

            # Create and return a timedelta object
            return timedelta(days=days, hours=hours, minutes=minutes, seconds=seconds)

        logging.warning("Could not parse uptime string: %r", uptime_str)
        return None

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
            r"dolos-V(\d+)-(\d{2})(\d{2})-([\dA-F]{4})$", dolos_serial_number
        )
        if match:
            version = match.group(1)
            year = match.group(2)
            week = match.group(3)
            number = int(match.group(4), 16)
            return f"DOLOSV{version}-C-{week}20{year}{number:04d}"

        return dolos_serial_number

    @property
    def serial(self):
        """Fetch the serial number if not present and return it."""
        if self._serial is None:
            try:
                self.get_status()
            except DolosConsoleError as err:
                logging.warning("Failed to load serial %s", err)
        return self._serial

    def _get_firmware_updater(self, temp_directory: tempfile.TemporaryDirectory):
        """Download the fw-updater tool and make it executable.

        Fetches the updater from the public Google Cloud Storage bucket and saves
        it to the provided temporary directory. It then sets the execute permission
        on the downloaded file.

        Args:
            temp_directory (tempfile.TemporaryDirectory): The directory where the
                updater will be downloaded.
        """

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

    def _get_binary(self, file_name: str, version: str):
        """Download a specific firmware or bootloader binary.

        Fetches the specified version of the firmware or bootloader from the
        public Google Cloud Storage bucket and saves it to a local file.

        0.x is legacy firmware version
        1.x is new firmware version
        2.x is bootloader version
        3.x is combined image version

        Args:
            file_name (str): The local path to save the downloaded binary.
            version (str): The version of the binary to download.
            bootloader (bool): If True, downloads the bootloader; otherwise,
                            downloads the firmware.
        """
        # TODO(b/438110087): Change file paths when removing support for legacy
        if version.startswith("0.") or version.startswith("1."):
            final_url = FIRMWARE_URL % {"firmware_version": version}
        elif version.startswith("2."):
            final_url = BOOTLOADER_URL % {"bootloader_version": version}
        else:
            final_url = COMBINED_URL % {"combined_version": version}
        req = requests.get(
            final_url,
            stream=True,
            timeout=180,
        )
        with open(file_name, "wb") as download_file:
            for chunk in req.iter_content(chunk_size=128):
                download_file.write(chunk)

    def _run_update(
        self,
        temp_directory: tempfile.TemporaryDirectory,
        file_name: str,
        bsl_mode: bool,
    ):
        """Run the fw-updater tool.

        This method temporarily closes the serial connection to free up the port,
        then executes the fw-updater with the provided binary. It logs the
        output and raises a DolosConsoleError on failure. The serial connection
        is reopened regardless of the outcome.

        Args:
            temp_directory (tempfile.TemporaryDirectory): The directory containing
                the fw-updater executable.
            file_name (str): The path to the firmware or bootloader binary to flash.
            bsl_mode (bool): If True, the --bsl-mode flag is passed to the updater.

        Raises:
            DolosConsoleError: If the updater subprocess returns a non-zero exit code.
        """
        try:
            self.close()
            cmd = [
                f"{temp_directory}/fw-updater",
                "-vvv",
                "--uart",
                SERIAL_PATH + self.uartname,
            ]

            if bsl_mode:
                cmd.append("--bsl-mode")

            cmd.append(file_name)

            proc = subprocess.run(
                cmd,
                check=True,
                capture_output=True,
            )
            logging.debug(
                "Firmware update %r, stdout: %r ,stderr: %r",
                " ".join(cmd),
                proc.stdout,
                proc.stderr,
            )
        except subprocess.CalledProcessError as err:
            logging.debug(str(err))
            logging.debug(err.stderr)
            logging.debug(err.stdout)
            raise DolosConsoleError(str(err)) from err
        finally:
            self.open()

    def update_firmware(self, firmware_version, bsl_mode=False):
        """Update the Dolos firmware to a different version.

        Both the updater and the firmware are posted on a public Google Cloud bucket,
        fetch both into a temporary directory and run the updater.

        Both the updater and firmware are very small so cost of download is not high
        enough to warrant a complex caching strategy.

        Args:
            firmware_version (string): version of the firmware to update to.
            bsl_mode (bool): True if you wish the updater to use alternative
                             bootloader.
        """
        if os.path.exists(TEMP_DIR):
            prefix = TEMP_DIR
        else:
            prefix = None
        with tempfile.TemporaryDirectory(prefix=prefix) as temp_directory:
            self._get_firmware_updater(temp_directory)
            # TODO(b/438110087): Change file paths when removing support for legacy
            if firmware_version.startswith("3."):
                dolo_hex_firmware_filename = f"{temp_directory}/combined.txt"
            else:
                dolo_hex_firmware_filename = f"{temp_directory}/zephyr.txt"
            self._get_binary(dolo_hex_firmware_filename, firmware_version)
            self._run_update(temp_directory, dolo_hex_firmware_filename, bsl_mode)

    def update_firmware_local(self, image_path, bsl_mode=False):
        """Update the Dolos firmware using a local image.

        Args:
            image_path (string): path to binary to update with.
            bsl_mode (bool): True if you wish the updater to use alternative
                             bootloader.
        """
        if os.path.exists(TEMP_DIR):
            prefix = TEMP_DIR
        else:
            prefix = None
        with tempfile.TemporaryDirectory(prefix=prefix) as temp_directory:
            self._get_firmware_updater(temp_directory)
            self._run_update(temp_directory, image_path, bsl_mode)

    def update_bootloader(self, bootloader_version, bsl_mode=False):
        """Update the Dolos bootloader to a different version.

        Both the updater and the bootloader are posted on a public Google Cloud bucket,
        fetch both into a temporary directory and run the updater.

        Both the updater and bootloader are very small so cost of download is not high
        enough to warrant a complex caching strategy.

        Args:
            bootloader_version (string): version of the bootloader to update to.
            bsl_mode (bool): True if you wish the updater to use BSL
                             bootloader.
        """
        if os.path.exists(TEMP_DIR):
            prefix = TEMP_DIR
        else:
            prefix = None
        with tempfile.TemporaryDirectory(prefix=prefix) as temp_directory:
            self._get_firmware_updater(temp_directory)
            dolos_hex_bootloader_filename = f"{temp_directory}/boot.txt"
            self._get_binary(dolos_hex_bootloader_filename, bootloader_version)
            self._run_update(temp_directory, dolos_hex_bootloader_filename, bsl_mode)

    def update_bootloader_local(self, bootloader_path, bsl_mode=False):
        """Update the Dolos bootloader using a local image

        Args:
            bootloader_path (string): path to binary to update with
            bsl_mode (bool): True if you wish the updater to use BSL
                             bootloader.
        """
        if os.path.exists(TEMP_DIR):
            prefix = TEMP_DIR
        else:
            prefix = None
        with tempfile.TemporaryDirectory(prefix=prefix) as temp_directory:
            self._get_firmware_updater(temp_directory)
            self._run_update(temp_directory, bootloader_path, bsl_mode)

    def determine_status(self, status_dict):
        """The external status of the device is abstracted from the internal status.

        This maps status command values to the correct external error status.

        There are other error status's that are not dependent on the console status
        output they are implemented elsewhere.

        Args:
            status_dict (dict{string: string}): Mapping of status console keys to
            values.

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

    def _decode_eeprom(self, response):
        """Decode the read eeprom readall response.

        Decode the response from the 'eeprom readall' command. Extracts
        the binary data from the message and verifies it is complete.

        Args:
            response: Console response to the read request.

        Returns:
            bytes: 1024B data segment of the EEPROM flash

        Raises:
            DolosConsoleError: Decoding showed a validation failure
        """
        data = b""
        for line in response.splitlines():
            m = re.search(
                r"(?P<addr>[\da-f]+)\s*:\s*(?P<data>([\da-f]{2}\s+){16})\s*\|",
                line,
                re.IGNORECASE,
            )
            if m is None:
                continue

            addr = int(m.group("addr"), 16)
            segment = bytes([int(x, 16) for x in m.group("data").split()])
            if len(data) != addr:
                break
            data += segment
        if len(data) != EEPROM_SIZE:
            raise DolosConsoleError(
                f"Incomplete eeprom read {len(data)}B of {EEPROM_SIZE}B",
            )
        return data

    def eeprom_read(self, retry_cnt=5):
        """Read the eeprom data.

        Args:
            retry_cnt: Number of times to attempt

        Returns:
            bytes: 1024B data segment of the EEPROM flash

        Raises:
            DolosConsoleError: Read failed.
        """

        for i in range(retry_cnt):
            try:
                # EEPROM commands tend to be slow.
                response = self.run_firmware_command(
                    "eeprom readall", timeout=5, echo_timeout=2
                )
                return self._decode_eeprom(response)
            except DolosConsoleError as err:
                logging.warning("EEPROM Read attempt %d error: %r", i, err)
        raise DolosConsoleError(f"Failed to read eeprom after {retry_cnt} attempts")

    def _eeprom_write_segment(self, addr, segment, retry_cnt=5):
        """Write and verify a segment is transferred.

        Write 16B long block of data to the EEPROM

        Args:
            addr (integer): Address of the data segment in 16 byte increments
            segment (bytes): 16B Data segment

        Raises:
            DolosConsoleError: We failed to write the segment
        """

        if addr < 0 or EEPROM_SIZE <= addr:
            raise DolosConsoleError(f"Invalid address {addr}")

        if len(segment) > EEPROM_WRITE_SIZE:
            raise DolosConsoleError(f"Invalid cunk size {len(segment)}")

        segment_hex = " ".join([hex(x) for x in segment])
        cmd = f"eeprom writen {hex(addr)} {len(segment)} {segment_hex}"

        for i in range(retry_cnt):
            try:
                # EEPROM commands tend to be slow.
                response = self.run_firmware_command(cmd, timeout=2, echo_timeout=2)
                m = re.search(r"address=(?P<addr>0x[\da-f]+)", response, re.IGNORECASE)
                if m is None:
                    raise DolosConsoleError("Invalid response")
                if int(m.group("addr"), 16) != addr:
                    raise DolosConsoleError(
                        "Incorrect response address",
                    )
                return
            except DolosConsoleError as err:
                logging.warning("EEPROM write segment attempt %d error: %r", i, err)
        raise DolosConsoleError(f"Failed to write eeprom after {retry_cnt} attempts")

    def _optimize_writes(self, current, target):
        """Optimize the writes by finding deltas and merging segments

        Args:
            current (bytes): Current data array
            target (bytes): Target data array
        """
        segments = []
        cur_seg = None
        for i in range(EEPROM_SIZE):
            if cur_seg is None:
                # No active segments are present
                if current[i] == target[i]:
                    continue

                # Start a new segment
                cur_seg = [i, i]
                segments.append(cur_seg)
            else:
                # If the delta continues then update the end of the segmets
                if current[i] != target[i]:
                    cur_seg[1] = i
                # Cap the length of a segment at the max write
                length = 1 + i - cur_seg[0]
                if length == EEPROM_WRITE_SIZE:
                    cur_seg = None
        return segments

    def eeprom_write(self, data, retry_cnt=5):
        """Write to the EEPROM cable and verify program is successful.

        Read the EEPROM data. If it matches, skip programming.
        Write the EEPROM and verify. If a failure happens then it will
        reattempt retry_cnt times.

        After it successfully programs, it will reset the Dolos to apply
        the changes.

        Args:
            data (bytes): 1024B data segment to write
            retry_cnt: Number of times to attempt

        Raises:
            DolosConsoleError: Write and verify failed after the retries.
        """

        if len(data) != EEPROM_SIZE:
            raise DolosConsoleError(
                f"Invalid eeprom size {len(data)}B of {EEPROM_SIZE}B",
            )

        read_data = self.eeprom_read()
        if data == read_data:
            logging.info("EEPROM already programmed")
            return

        for i in range(retry_cnt):

            segments = self._optimize_writes(read_data, data)

            try:
                for seg in segments:
                    data_seg = data[seg[0] : seg[1] + 1]
                    self._eeprom_write_segment(seg[0], data_seg)

                read_data = self.eeprom_read()
                if data == read_data:
                    logging.info("EEPROM verified")
                    logging.info("Resetting Dolos to apply changes")

                    # Reboot the device to load the new configs
                    self.run_firmware_command("reset", timeout=5)
                    return

            except DolosConsoleError as err:
                logging.warning("EEPROM write verify attempt %d error: %r", i, err)
        raise DolosConsoleError(
            f"Failed to write and verify eeprom after {retry_cnt} attempts"
        )

    def program_cable(self, model_hwid, file=None, new_serial=None):
        """Program the Dolos cable

        [description]

        Args:
            model_hwid ([type]): hwid or model name
            file ([type]): Path to config file (default: `None`)
            new_serial ([type]): New dolos serial (default: `None`)
        """

        # Generate the new config table
        config = Config(model_hwid)
        config.load_config(file)
        table = config.extract_table()

        read_data = self.eeprom_read()
        eeprom = EEPROMLayout(read_data)

        if new_serial:
            eeprom.update_serial(new_serial)

        write_data = eeprom.create_payload(table)

        self.eeprom_write(write_data)
