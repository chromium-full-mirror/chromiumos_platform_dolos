# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=too-few-public-methods

"""Common utilities for interacting with I2C bridges."""

import logging
import re

import pyftdi.ftdi
import pyftdi.i2c


class I2CException(Exception):
    """General exception for I2C errors."""


class I2CFTDI:
    """Handles I2C transfers using a FTDI USB-I2C interface."""

    def __init__(self):
        """Init FTDI USB-I2C device.

        This connects to a PyFTDI compatible I2C device and configures it.
        1 FTDI 4232H must be present to work.

        Raises:
            I2CException: The correct FTDI I2C device was not identified
        """
        try:
            self.i2c = pyftdi.i2c.I2cController()
            self.i2c.configure("ftdi://ftdi:4232h/1", frequency=100e3)
        except pyftdi.usbtools.UsbToolsError as err:
            raise I2CException() from err

    def transfer(self, write_data, read_len, addr7):
        """Perform I2C Transfer using the FTDI board

        Args:
          write_data: Data to write.
          read_len: Number of bytes to read.
          addr7: I2C Address in 7-bit format

        Returns:
          bytes object representing read payload

        Raises:
            I2CException: Transfer failure
        """

        logging.debug(
            "I2C Transfer Start - Addr7:%d, write_data:%r, read_len:%d",
            addr7,
            write_data,
            read_len,
        )
        device = self.i2c.get_port(addr7)
        read_data = b""
        try:
            if read_len == 0:
                device.write(write_data)
            else:
                read_data = device.exchange(write_data, read_len)
        except pyftdi.i2c.I2cNackError as err:
            raise I2CException() from err
        logging.debug("I2C Transfer Stop - read_data:%r", read_data)
        return read_data


class ECToolI2C:
    """Handles I2C transfers using a shell and ECTool"""

    def __init__(self, proc, port=None):
        self.port = port
        self._shell = proc

    def transfer(self, write_data, read_len, addr7):
        """Perform I2C Transfer using ECTool.

        This is designed to work with the SSH shells

        Args:
          write_data: Data to write.
          read_len: Number of bytes to read.
          addr7: I2C Address in 7-bit format

        Returns:
            bytes: Read data

        Raises:
            I2CException: Transfer failure
        """

        logging.debug(
            "I2C Transfer Start - Port %d, Addr7:%d, write_data:%r, read_len:%d",
            self.port,
            addr7,
            write_data,
            read_len,
        )
        cmd = self._ec_cmd(addr7, write_data, read_len)

        # Run the command.
        response = self._shell.run(cmd)

        read_data = self._ec_result(response, read_len)
        logging.debug("I2C Transfer Stop - read_data:%r", read_data)
        return read_data

    def _ec_cmd(self, addr7, write_data, read_len):
        """Generate the I2C transfer command for ectool.

        Args:
            addr7: I2C Address in 7-bit format
            write_data: bytes object with the write payload
            read_len: Number of bytes to read
        Returns:
            string: Ectool command to perform the transfer
        """
        write_data = " ".join([str(x) for x in list(write_data)])
        return f"ectool i2cxfer {self.port} {addr7} {read_len} {write_data}"

    def _ec_result(self, response, read_len):
        """Parse the ectool I2C transfer response

        Args:
          response: Response text
          read_len: Number of bytes to read.

        Returns:
            bytes: Read data

        Raises:
            I2CException: Transfer failure"""
        match = re.search(r"Read bytes:(.*)", response)
        if not match:
            raise I2CException(f"Result failed {response}")
        data = match[1].split()
        data = bytes([int(x, 16) for x in data])
        if len(data) != read_len:
            text = f"Result length error Expected:{read_len}, Actual:{len(data)}"
            logging.error(text)
            raise I2CException(text)
        return data
