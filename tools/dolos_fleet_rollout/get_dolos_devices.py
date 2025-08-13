#!/usr/bin/python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Get a list of Dolos devices from the fleet."""

import argparse
import csv
import random
import subprocess

from env_utils import run_initial_env_checks


def get_dolos_devices_query():
    """Returns the SQL query to get Dolos devices."""
    return (
        "SELECT dut_info.hostname as hostname, "
        "dut_info.servo_hostname as labstation, "
        "dut_info.pool as pool, "
        "dut_info.model as model, "
        "dut_info.hwid as hwid, "
        "dut_info.state as state, "
        "dut_info.dolos_state as dolos_state, "
        "inventory_info.dolos_serial_usb as uartname, "
        "inventory_info.dolos_serial_cable as cable_serial, "
        "inventory_info.rpm_powerunit_outlet as rpm_outlet, "
        "dut_info.board as board, "
        "inventory_info.dolos_fw_version as dolos_fw_version "
        "FROM chrome_fleet_analytics.cros_fleet.latest_dut_info AS dut_info "
        "LEFT JOIN chrome_fleet_analytics.cros_fleet.latest_inventory_info "
        "AS inventory_info ON dut_info.hostname = inventory_info.hostname "
        "WHERE inventory_info.dolos_serial_cable IS NOT NULL AND "
        'inventory_info.dolos_serial_cable != "";'
    )


def get_all_devices():
    """Gets all Dolos devices from the fleet."""
    query = get_dolos_devices_query()
    f1_sql_command = ["f1-sql", "--csv_output"]
    print("Querying the fleet database for all Dolos devices...")
    output = subprocess.check_output(
        f1_sql_command, input=query, text=True, encoding="utf-8"
    )
    reader = csv.reader(output.strip().splitlines())
    header = next(reader)
    devices = [dict(zip(header, row)) for row in reader]
    return devices


def group_by_board(devices):
    """Groups a list of devices by board."""
    by_board = {}
    for device in devices:
        board = device.get("board")
        if board not in by_board:
            by_board[board] = []
        by_board[board].append(device)
    return by_board


def get_staged_devices(stage, all_devices, set_version):
    """Returns a list of devices for a given stage."""
    if set_version:
        devices_to_update = [
            d for d in all_devices if d.get("dolos_fw_version") != set_version
        ]
    else:
        devices_to_update = all_devices

    if stage == "all":
        return devices_to_update

    total_devices = len(devices_to_update)
    if stage == "first":
        by_board = group_by_board(devices_to_update)
        staged_devices = []
        # Add one device from each board
        for _, board_devices in by_board.items():
            staged_devices.append(random.choice(board_devices))

        # Add more random devices to reach ~10%
        remaining_devices = [d for d in devices_to_update if d not in staged_devices]
        target_count = int(total_devices * 0.1)
        if len(staged_devices) < target_count:
            needed = target_count - len(staged_devices)
            staged_devices.extend(
                random.sample(remaining_devices, min(needed, len(remaining_devices)))
            )
        return staged_devices

    if stage == "second":
        target_count = int(total_devices * 0.33)
        return random.sample(devices_to_update, min(target_count, total_devices))

    return []


def write_csv(devices, output_file):
    """Writes a list of devices to a CSV file."""
    if not devices:
        print("No devices to write.")
        return

    with open(output_file, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=devices[0].keys())
        writer.writeheader()
        writer.writerows(devices)


def main():
    """Main function."""
    run_initial_env_checks()
    parser = argparse.ArgumentParser(
        description="Get a list of Dolos devices from the fleet."
    )
    parser.add_argument(
        "--output-file", required=True, help="Path to the output CSV file."
    )
    parser.add_argument(
        "--stage",
        choices=["first", "second", "all"],
        default="all",
        help="The rollout stage.",
    )
    parser.add_argument(
        "--set-version",
        help="Target firmware version to filter out already updated devices.",
    )
    args = parser.parse_args()

    all_devices = get_all_devices()
    staged_devices = get_staged_devices(args.stage, all_devices, args.set_version)
    write_csv(staged_devices, args.output_file)

    print(f"Successfully wrote {len(staged_devices)} devices to {args.output_file}")


if __name__ == "__main__":
    main()
