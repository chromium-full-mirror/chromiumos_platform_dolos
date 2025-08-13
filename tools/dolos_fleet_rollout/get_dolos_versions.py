#!/usr/bin/python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks Dolos firmware versions for a fleet of devices."""

import argparse
import concurrent.futures
import csv
import re
import subprocess
import sys
import time

from env_utils import run_initial_env_checks


# --- Configuration ---
# Maximum number of parallel SSH connections
MAX_WORKERS = 10
# SSH connection timeout in seconds
SSH_CONNECT_TIMEOUT = 20
# SSH command execution timeout in seconds
SSH_COMMAND_TIMEOUT = 60
# --- End Configuration ---


def get_version(device_info):
    """
    Fetches Dolos firmware version for a single device.

    `device_info` is a dict with at least 'labstation' and 'cable_serial'.
    Returns a dict with original device_info, 'reported_version', and 'error'.
    """
    labstation = device_info.get("labstation")
    cable_serial = device_info.get("cable_serial")
    hostname = device_info.get("hostname", "N/A")  # for logging

    if not labstation or not cable_serial:
        print(
            f"[{hostname}] Skipping device, missing 'labstation' or 'cable_serial'.",
            file=sys.stderr,
        )
        return {
            **device_info,
            "reported_version": None,
            "error": "Missing labstation or cable_serial",
        }

    print(f"[{hostname}] Querying version from {labstation}...")
    start_time = time.time()

    command = f"doloscmd version --serial {cable_serial}"
    ssh_command = [
        "ssh",
        "-o",
        f"ConnectTimeout={SSH_CONNECT_TIMEOUT}",
        "-o",
        "BatchMode=yes",
        f"root@{labstation}",
        command,
    ]

    try:
        result = subprocess.run(
            ssh_command,
            capture_output=True,
            text=True,
            check=True,
            timeout=SSH_COMMAND_TIMEOUT,
            encoding="utf-8",
        )
        # Regex to find a version string like '1.2.3-abc1234'
        version_match = re.search(r"(\d+\.\d+\.\d+-\w+)", result.stdout)
        if not version_match:
            duration = time.time() - start_time
            error_msg = "Could not parse version from output"
            print(
                f"[{hostname}] Failed! {error_msg} ({duration:.2f}s)",
                file=sys.stderr,
            )
            if result.stdout:
                print(
                    f"[{hostname}] stdout:\n{result.stdout.strip()}",
                    file=sys.stderr,
                )
            return {
                **device_info,
                "reported_version": None,
                "error": f"{error_msg}: {result.stdout.strip()}",
            }

        version = version_match.group(1)
        duration = time.time() - start_time
        print(f"[{hostname}] Success. Version: {version} ({duration:.2f}s)")
        return {**device_info, "reported_version": version, "error": None}

    except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as e:
        duration = time.time() - start_time
        error_msg = f"SSH command failed: {e}"
        print(f"[{hostname}] Failed! {error_msg} ({duration:.2f}s)", file=sys.stderr)
        if hasattr(e, "stdout") and e.stdout:
            print(f"[{hostname}] stdout:\n{e.stdout.strip()}", file=sys.stderr)
        return {**device_info, "reported_version": None, "error": str(e)}


def read_devices_from_csv(csv_file):
    """Reads device info from a CSV file."""
    devices = []
    try:
        with open(csv_file, "r", newline="", encoding="utf-8") as file:
            reader = csv.DictReader(file)
            if not reader.fieldnames or not {"labstation", "cable_serial"}.issubset(
                reader.fieldnames
            ):
                print(
                    "Error: CSV file must contain 'labstation' "
                    "and 'cable_serial' columns.",
                    file=sys.stderr,
                )
                sys.exit(1)
            for row in reader:
                if row.get("labstation") and row.get("cable_serial"):
                    devices.append(row)
                else:
                    print(
                        f"WARNING: {row.get("hostname")} is missing "
                        "labstation or serial.",
                        file=sys.stderr,
                    )
    except FileNotFoundError:
        print(f"Error: CSV file not found at: {csv_file}", file=sys.stderr)
        sys.exit(1)
    except IOError as e:
        print(f"Error reading CSV file {csv_file}: {e}", file=sys.stderr)
        sys.exit(1)
    return devices


def process_results(all_results, devices, set_version):
    """Processes the results of the version checks."""
    updated_count = 0
    mismatched_devices = []
    failed_count = 0

    # The results from executor.map are in the same order as the input `devices`
    for i, r in enumerate(all_results):
        if r.get("reported_version"):
            if r["reported_version"] == set_version:
                updated_count += 1
            else:
                mismatched_devices.append(devices[i])
        else:
            failed_count += 1
            mismatched_devices.append(devices[i])
    return updated_count, mismatched_devices, failed_count


def write_mismatched_to_csv(mismatched_devices, output_csv, fieldnames):
    """Writes the list of mismatched devices to a CSV file."""
    try:
        with open(output_csv, "w", newline="", encoding="utf-8") as outfile:
            writer = csv.DictWriter(outfile, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(mismatched_devices)
        print(
            f"\nSuccessfully wrote {len(mismatched_devices)} mismatched devices to "
            f"{output_csv}"
        )
    except IOError as e:
        print(
            f"Error writing to output file {output_csv}: {e}",
            file=sys.stderr,
        )


def main():
    """Main function."""
    run_initial_env_checks()
    parser = argparse.ArgumentParser(
        description="Get Dolos firmware versions for a fleet of devices."
    )
    parser.add_argument(
        "--input-csv",
        required=True,
        help="Path to the input CSV file with device info.",
    )
    parser.add_argument(
        "--output-csv",
        required=True,
        help="Path to the output CSV file to store results.",
    )
    parser.add_argument(
        "--set-version",
        required=True,
        help="The expected firmware version to check against.",
    )
    args = parser.parse_args()

    devices = read_devices_from_csv(args.input_csv)
    if not devices:
        print("No valid devices found in the CSV file.")
        return

    print(
        f"Found {len(devices)} valid devices "
        f"to process with up to {MAX_WORKERS} workers."
    )
    total_start_time = time.time()

    all_results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        results_iterator = executor.map(get_version, devices)
        all_results = list(results_iterator)

    updated_count, mismatched_devices, failed_count = process_results(
        all_results, devices, args.set_version
    )

    if mismatched_devices:
        write_mismatched_to_csv(
            mismatched_devices, args.output_csv, list(devices[0].keys())
        )
    else:
        print("\nNo mismatched devices found. Output CSV not created.")

    total_duration = time.time() - total_start_time
    print(f"\nTotal execution time: {total_duration:.2f} seconds")

    print("\n--- Version Rollout Summary ---")
    print(f"Expected version: {args.set_version}")
    print(f"Total devices checked: {len(all_results)}")
    print(f"Devices with expected version: {updated_count}")
    print(f"Devices with different version: {len(mismatched_devices) - failed_count}")
    print(f"Devices where version check failed: {failed_count}")
    if all_results:
        progress = (updated_count / len(all_results)) * 100
        print(f"Rollout progress: {progress:.2f}%")


if __name__ == "__main__":
    main()
