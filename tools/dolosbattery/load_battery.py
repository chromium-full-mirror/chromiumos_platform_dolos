#!/usr/bin/python3
# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=too-few-public-methods

"""Provides a command line interface loading battery registers."""

import argparse
import logging
import pathlib
import re

from dolosbattery import i2c_interface
from dolosbattery import logs
from dolosbattery import shell_interface
from doloscmd import registers
import yaml


class LoadBatteryException(Exception):
    """General exception errors when loading the battery registers."""


class BatteryRegisters:
    """Handles the procedure for identifying the battery and loading the registers."""

    I2C_PORT_BATTERY = 0x0B

    def __init__(self, i2c, shell):
        self._i2c = i2c
        self._shell = shell
        self._ectool = None
        if isinstance(i2c, i2c_interface.ECToolI2C):
            self._ectool = i2c
            if self._i2c.port is None:
                self._configure_port()

    def _read_field(self, field):
        """Read each field and call the correct handler.

        Args:
            field (dict): Field attributes

        Returns:
            int or bytes: Depending on field type, returns int or bytes object

        Raises:
            I2CException: I2C transfer failed
        """
        reg = field["reg"]
        if "word" in field:
            data = self._i2c.transfer([reg], 2, addr7=self.I2C_PORT_BATTERY)
            return data[1] << 8 | data[0]
        # Handle block data
        data = self._i2c.transfer([reg], 33, addr7=self.I2C_PORT_BATTERY)
        length = data[0]
        return data[1 : length + 1]

    def _ectool_battery_props(self):

        cmd = "ectool battery"

        # Run the command.
        response = self._shell.run(cmd)

        # Search for error conditions
        match = re.search(r"Battery version \d+ is not supported", response)
        if match:
            raise LoadBatteryException("ECTool - Invalid battery version")
        match = re.search(r"Cannot find I2C adapter", response)
        if match:
            raise LoadBatteryException("ECTool - No I2C adapter")

        matchers = [
            ("SB_DEVICE_CHEMISTRY", str, r"Chemistry\s*:\s*(.*)"),
            ("SB_CYCLE_COUNT", int, r"Cycle count\s+(\d+)"),
            ("SB_DESIGN_VOLTAGE", int, r"Design output voltage\s+(\d+) mV"),
            ("SB_DESIGN_CAPACITY", int, r"Design capacity:\s+(\d+)"),
        ]
        table = {}
        for key, cast, reg in matchers:
            match = re.search(reg, response)
            if match:
                value = match[1]
                table[key] = cast(value)
            else:
                raise LoadBatteryException(f"Unable to find battery attribute {key}")
        return table

    def _check_port(self, battery_props, port, quick=True):
        """Read the I2C registers and compare against battery properties.

        Read the I2C registers from a port and check for a match. Supports
        a quick and detailed check mode.

        Args:
            battery_props: Dictionary of battery properties
            port : I2C port to test
            quick: Controls if we use the quick or detailed check

        Returns:
            bool: True if we match all checks
        """
        self._i2c.port = port
        if quick:
            try:
                chem = self._read_field(registers.find_register("SB_DEVICE_CHEMISTRY"))
                chem = chem.decode(errors="replace")
            except i2c_interface.I2CException:
                return False
            if chem == battery_props["SB_DEVICE_CHEMISTRY"]:
                return True
        else:
            # Some batteries seem to have no visible strings.
            # We'll compare several fields to see if we can find a likely match.
            validation_registers = [
                "SB_CYCLE_COUNT",
                "SB_DESIGN_CAPACITY",
                "SB_DESIGN_VOLTAGE",
            ]
            for name in validation_registers:
                register = registers.find_register(name)
                try:
                    value = self._read_field(register)
                except i2c_interface.I2CException:
                    return False
                if battery_props[name] != value:
                    return False
            return True
        return False

    def _configure_port(self):
        """Find the I2C port used by the battery and configure the port

        Reads the battery properties and scan each I2C port to find the battery.
        Only a few chips share the same I2C address as batteries in the EC repo
        but it's non-zero and we want to minimize transactions. The quick mode
        checks the battery chemistry which works on ~98% of DUTs but sometimes
        a battery does not support the field.

        If the check fails. A detailed check can be done on a few additional
        registers which are expected to remain constant over a few minutes.

        Returns:
            int: I2C port number
        Raises:
            LoadBatteryException: No port found
        """
        max_i2c_port = 20
        battery_props = self._ectool_battery_props()
        logging.info("Detecting port")
        for port in range(max_i2c_port + 1):
            logging.debug("Checking port %d", port)
            if self._check_port(battery_props, port, True):
                logging.info("Using port %d", port)
                return port
        logging.debug("Quick method failed, trying slower comparision")
        for port in range(max_i2c_port + 1):
            logging.debug("Checking port %d", port)
            if self._check_port(battery_props, port, False):
                logging.info("Using port %d", port)
                return port
        raise LoadBatteryException("No port found")

    def load_registers(self):
        """Load the battery registers.

        Load each register and store it's results in a table

        Returns:
            dict: Mapping of all registers and their values
        """
        logging.info("Loading registers")
        table = {}
        for i, field in enumerate(registers.REGISTERS, 1):
            name = field["name"]
            logging.info(
                "Progress %d/%d - Loading %r", i, len(registers.REGISTERS), name
            )
            try:
                value = self._read_field(field)
            except i2c_interface.I2CException:
                value = None
            table[name] = value
        return table


def export_table(file, table):
    """Export the table as a yaml file."""
    logging.info("Exporting yaml to:%r", file)

    # Declare some helpers to format the data a bit better
    def str_repr(dumper, data):
        style = "|" if "\n" in data else None
        return dumper.represent_scalar("tag:yaml.org,2002:str", data, style=style)

    def bytes_repr(dumper, data):
        return dumper.represent_list(list(data))

    yaml.representer.SafeRepresenter.add_representer(str, str_repr)
    yaml.representer.SafeRepresenter.add_representer(bytes, bytes_repr)

    text = yaml.safe_dump(table, default_flow_style=None, sort_keys=False)

    if file is not None:
        logging.debug("Yaml data %r", text)
        file.write_text(text)
        logging.debug("Yaml data written to %r", file)
    else:
        logging.debug(text)


def configure_connections(ftdi_i2c=None, ssh=None, i2c_port=None):
    """Configure the connections for the I2C and shell."""
    i2c = None
    shell = None
    if ftdi_i2c:
        logging.debug("Using FTDI device")
        try:
            i2c = i2c_interface.I2CFTDI()
        except i2c_interface.I2CException as e:
            raise LoadBatteryException(
                "Unable to connect to FTDI, is it connected?"
            ) from e
    else:
        logging.debug("Connecting to SSH target %r", ssh)
        shell = None
        try:
            shell = shell_interface.SshShell(ssh)
        except shell_interface.SSHException as err:
            raise LoadBatteryException(
                "Connection failed, verify glogin and address"
            ) from err

        i2c = i2c_interface.ECToolI2C(shell, port=i2c_port)
    return i2c, shell


def main():
    """Parse the args, load the battery registers, and save the table.

    Raises:
        LoadBatteryException() with diagnostic details on failure.
    """
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "-s",
        dest="ssh",
        help="Use SSH and ECTool to extract registers from a remote DUT",
    )
    group.add_argument(
        "-i",
        dest="ftdi_i2c",
        action="store_true",
        help="Use a FTDI board to extract registers from an unplugged battery",
    )
    parser.add_argument(
        "-p",
        dest="i2c_port",
        type=int,
        help="Configure ECTool's I2C port if I2C_PORT_BATTERY is known",
    )
    parser.add_argument(
        "-f",
        dest="file",
        type=pathlib.Path,
        required=True,
        help="File path to export yaml data",
    )
    parser.add_argument(
        "-c",
        dest="comment",
        default="comment section",
        help="Optional comment data",
    )
    parser.add_argument(
        "-v", dest="verbose", action="store_true", help="Enable verbose logging"
    )
    args = parser.parse_args()
    logs.set_config(args.verbose)
    i2c, shell = configure_connections(args.ftdi_i2c, args.ssh, args.i2c_port)
    table = {"Comment": args.comment, "Version": 1, "Polarity": "unknown"}
    battery = BatteryRegisters(i2c, shell)
    table |= battery.load_registers()
    export_table(args.file, table)


if __name__ == "__main__":
    main()
