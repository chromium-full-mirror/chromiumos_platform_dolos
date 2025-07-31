"""Python script to run factory tests on the connected Dolos"""

# Lint as: python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import argparse
import getopt
import os
import re
import subprocess
import sys
import time

import pytest
import serial.tools.list_ports


def run_all_pytests(serial_port, baudrate, is_zephyr=False):
    """Runs all the pytests with the given serial and baudrate"""
    args = [
        f"{os.path.dirname(os.path.abspath(__file__))}/test_factory.py",
        "-rA",
        "--uart",
        serial_port,
        "--baudrate",
        str(baudrate),
    ]
    if is_zephyr:
        args.append("--zephyr")
    pytest.main(args)


def run_pytests_without_eeprom(serial_port, baudrate, is_zephyr=False):
    """Runs all pytests except eeprom test with given serial and baudrate"""
    args = [
        "-k",
        "not test_eeprom",
        f"{os.path.dirname(os.path.abspath(__file__))}/test_factory.py",
        "-rA",
        "--uart",
        serial_port,
        "--baudrate",
        str(baudrate),
    ]
    if is_zephyr:
        args.append("--zephyr")
    pytest.main(args)


def send_command(cmd, serial_port):
    """Sends CLI to Dolos.

    Sends given command to Firmware using the given serial_port
    and returns the output as string
    """
    serial_port.write(str.encode(f"{cmd}\r"))
    lines = []
    while True:
        line = serial_port.readline().decode("utf-8")
        line = re.sub(r"\s+", " ", line)
        if len(line) != 0:
            lines.append(line.strip())
        else:
            break
    return lines


def get_serial_number(serial_port):
    """Function to get the serial number of the Dolos.

    Sends get-serial-no command to firmware and returns the response
    as string if there is no error, otherwise it returns ERROR
    """
    output = send_command("get-serial-no", serial_port)
    if any("ERROR" in s for s in output):
        return "ERROR"
    serial_no = [element for element in output if element.startswith("DOLOS")]
    return serial_no


def get_version(serial_port):
    """Sends version command to firmware and returns the response as string"""
    output = send_command("version", serial_port)
    version_elements = [element for element in output if element.startswith("version")]
    for element in version_elements:
        if element != "version":
            return element


def fw_update():
    # Get the path to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    # Path to the Rust executable
    updater_executable = os.path.join(script_dir, "fw-updater")

    # Check if the executable exists
    if not os.path.exists(updater_executable):
        print("Error: fw-updater executable not found.")
        return 1

    # Get the path to the dolos.txt file in the package
    dolos_txt_path = os.path.join(script_dir, "dolos.txt")

    # Run the Rust executable with dolos.txt as parameter
    try:
        subprocess.run([updater_executable, dolos_txt_path], check=True)
    except subprocess.CalledProcessError as e:
        print("Error running fw-updater:", e)
        return 1


def main():
    # Running fw-updater to flash dolos.
    print("===Start Flashing Dolos===")
    fw_update()
    print("===Flashing Dolos done successfully===")

    """Entry point of the script.

    Initializes the baudrate option with initial value = 115200,
     keeps reading the serial ports connected to see
    if there is any Dolos connected to run tests on it
    """
    baudrate: int = 115200
    zephyr_mode = False
    parser = argparse.ArgumentParser(
        description="Run factory tests on the connected Dolos"
    )
    parser.add_argument("--baudrate", type=int, default=115200, help="Set the baudrate")
    parser.add_argument(
        "--zephyr", action="store_true", help="Run Zephyr-specific tests"
    )
    args = parser.parse_args()

    baudrate = args.baudrate
    zephyr_mode = args.zephyr
    print(f"baudrate is {baudrate}")
    print("zephyr mode is:", zephyr_mode)
    ports = serial.tools.list_ports.comports()
    print("serial ports:-")
    for serial_port in ports:
        print(
            f"{serial_port.device} : {serial_port.description} "
            f"{serial_port.vid} "
            f"{serial_port.pid} {serial_port.manufacturer}"
            f" {serial_port.product} {serial_port.location}"
        )
    while True:
        for port in ports:
            if port.manufacturer == "FTDI":
                print(f"New Dolos device detected: {port}")
                try:
                    serial_port = serial.Serial(
                        port.device,
                        baudrate=baudrate,
                        timeout=2,
                        write_timeout=1,
                    )
                    version = get_version(serial_port)
                    serial_no = get_serial_number(serial_port)
                    if serial_no == "ERROR":
                        run_pytests_without_eeprom(port.device, baudrate, zephyr_mode)
                    else:
                        run_all_pytests(port.device, baudrate, zephyr_mode)
                        print(serial_no)
                    print(version)
                except Exception as e:
                    print(f"Error accessing device: {e}")
                print("Tests finished.\nPlease disconnect the dolos.")
                while True:
                    if port not in serial.tools.list_ports.comports():
                        print(f"{port} disconnected successfully")
                        break
        ports = serial.tools.list_ports.comports()
        time.sleep(1)


if __name__ == "__main__":
    main()
