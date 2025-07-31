"""Functions to access FW CLI over UART"""

# Lint as: python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys

import serial


class FwUart:
    """FWUart class containing function to access FW CLI over UART"""

    def __init__(self, uart_path, baudrate):
        """Initialization function.

        initializes the serial port with the given uart_path and
        baudrate, reading timeout will be 2 and writing timeout will be 1
        """
        self.serial_port = serial.Serial(
            port=uart_path, baudrate=baudrate, timeout=2, write_timeout=1
        )

    def send_command(self, cmd):
        """Sends given command to the firmware and returns output as string"""
        self.serial_port.write(str.encode(f"{cmd}\r"))
        lines = []
        while True:
            line = self.serial_port.readline().decode("utf-8")
            line = re.sub(r"\s+", " ", line)
            if len(line) != 0:
                lines.append(line.strip())
            else:
                break
        return lines

    def read_buck_boost_and_chrg_pin(self):
        """Function to read the buck boost and charge bin.

        read the response of the cli : smbus 6b r b 2e
        also check if the charger is plugged in through the pin chrg-ok.
        The response of (smbus 6b r b 2e) will be returned as string
        and the response of (gpio chrg-ok) will be returned as integer
        """
        output = self.send_command("smbus 6b r b 2e")
        byte_value = "".join(
            [s.split("byte=")[-1].strip() for s in output if "byte=" in s]
        )
        if "--zephyr" in sys.argv:
            chrg_output = self.send_command("gpio chrg_ok")
        else:
            chrg_output = self.send_command("gpio chrg-ok")
        chrg_ok_pin_value = int(
            [s.split(":")[-1].strip() for s in chrg_output if "CHRG_OK pin state" in s][
                0
            ]
        )
        return byte_value, chrg_ok_pin_value

    def read_efuse_pg_pin(self):
        """Function to read the efuse pin.

        Sends (gpio efuse-pg) command to firmware and
        returns EFSUE_PG pin state as integer.
        """
        if "--zephyr" in sys.argv:
            output = self.send_command("gpio efuse_pg")
        else:
            output = self.send_command("gpio efuse-pg")

        efuse_pin = int(
            [s.split(":")[-1].strip() for s in output if "EFSUE_PG pin state" in s][0]
        )
        return efuse_pin

    def read_pac(self):
        """Function to read the pac.

        Sends pac command to firmware and returns the voltage and
        current values as integers.
        """
        output = self.send_command("pac")
        voltage = [
            int(s.split(":")[1][:-2]) for s in output if s.startswith("Voltage")
        ][0]
        current = [
            int(s.split(":")[1][:-2]) for s in output if s.startswith("Current")
        ][0]
        return voltage, current

    def read_temp(self):
        """Function to read the temperature in Celsius.

        Sends temp command to firmware and returns the
        temperature value in Celsius as float
        """
        output = self.send_command("temp")
        temp_in_cel = float(
            [s.split(":")[1] for s in output if s.startswith("Temperature in Celsius")][
                0
            ]
        )
        return temp_in_cel

    def read_smbus_stats(self):
        """Function to read the smbus stats.

        Sends stats smbus command to firmware and returns the values:
        SMBus Read success, Smart Battery Read fail and
        Smart Battery Write fail as integers.
        """
        output = self.send_command("stats smbus")
        smbus_read_success_value = int(
            [s.split(":")[-1].strip() for s in output if "SMBus Read success" in s][0]
        )
        smart_battery_read_fail_value = int(
            [
                s.split(":")[-1].strip()
                for s in output
                if "Smart Battery Read fail" in s
            ][0]
        )
        smart_battery_write_fail_value = int(
            [
                s.split(":")[-1].strip()
                for s in output
                if "Smart Battery Write fail" in s
            ][0]
        )
        return (
            smbus_read_success_value,
            smart_battery_read_fail_value,
            smart_battery_write_fail_value,
        )

    def read_eeprom(self):
        """Sends eeprom-read command and returns output as a list of strings"""
        output = self.send_command("eeprom-read")
        return output
