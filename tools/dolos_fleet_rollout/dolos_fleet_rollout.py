#!/usr/bin/python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dolos fleet rollout tool."""

import argparse
from collections import Counter
import concurrent.futures
import csv
from functools import partial
import json
import re
import sys

from env_utils import run_command
from env_utils import run_initial_env_checks


# Maximum number of parallel shivas connections
MAX_WORKERS = 5


def get_hostnames_from_csv(csv_file):
    """Reads hostnames from a CSV file."""
    hostnames = []
    with open(csv_file, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        next(reader)  # Skip header
        for row in reader:
            hostnames.append(row[0])
    return hostnames


def set_firmware_version_for_host(hostname, version):
    """Sets the dolos firmware version for a single host."""
    print(f"[{hostname}] Setting firmware version to {version}...")
    command = [
        "shivas",
        "update",
        "dut",
        "-name",
        hostname,
        "-dolos-firmware-version",
        version,
    ]
    result = run_command(command, check=False)
    if result.returncode == 0:
        print(f"[{hostname}] Success.")
        return None

    error_msg = f"Failed to set version for {hostname}: {result.stderr.strip()}"
    print(error_msg, file=sys.stderr)
    return error_msg


def set_firmware_version(hostnames, version):
    """Sets the dolos firmware version for a list of hosts in parallel."""
    print(
        f"Setting dolos firmware version to {version} for {len(hostnames)} hosts "
        f"with up to {MAX_WORKERS} workers..."
    )

    with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        worker = partial(set_firmware_version_for_host, version=version)
        results = list(executor.map(worker, hostnames))

    failures = [r for r in results if r is not None]
    if failures:
        print(f"\n{len(failures)} hosts failed to update.", file=sys.stderr)

    print("\nDone.")


def repair_dut(hostname):
    """Schedules a repair job for a single host."""
    print(f"[{hostname}] Scheduling repair...")
    command = ["shivas", "repair", hostname]
    result = run_command(command, check=False)
    if result.returncode == 0:
        print(f"[{hostname}] Success.")
        return None

    error_msg = f"Failed to schedule repair for {hostname}: {result.stderr.strip()}"
    print(error_msg, file=sys.stderr)
    return error_msg


def repair_duts(hostnames):
    """Schedules repair jobs for a list of hosts in parallel."""
    print(
        f"Scheduling repair for {len(hostnames)} hosts with up to {MAX_WORKERS} "
        "workers..."
    )

    with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        results = list(executor.map(repair_dut, hostnames))

    failures = [r for r in results if r is not None]
    if failures:
        print(f"\n{len(failures)} hosts failed to schedule repair.", file=sys.stderr)

    print("Done.")


def get_firmware_version_for_host(hostname):
    """Gets the dolos firmware version for a single host."""
    print(f"[{hostname}] Getting firmware version...")
    command = ["shivas", "get", "dut", "-json", hostname]
    result = run_command(command, check=False)
    if result.returncode != 0:
        error_msg = f"Failed to get info for {hostname}: {result.stderr.strip()}"
        print(error_msg, file=sys.stderr)
        return hostname, "ERROR"

    try:
        data = json.loads(result.stdout)
        version = (
            data[0]
            .get("chromeosMachineLse", {})
            .get("deviceLse", {})
            .get("dut", {})
            .get("peripherals", {})
            .get("dolos", {})
            .get("fwVersion", "N/A")
        )
        print(f"[{hostname}] Version: {version}")
        return hostname, version
    except (json.JSONDecodeError, IndexError, KeyError) as e:
        error_msg = f"Failed to parse JSON for {hostname}: {e}"
        print(error_msg, file=sys.stderr)
        return hostname, "ERROR"


def scan_firmware_versions(hostnames):
    """Gets and summarizes dolos firmware versions for a list of hosts."""
    print(
        f"Scanning firmware versions for {len(hostnames)} hosts with up to "
        f"{MAX_WORKERS} workers..."
    )

    versions = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        results = list(executor.map(get_firmware_version_for_host, hostnames))

    for hostname, version in results:
        versions[hostname] = version

    print("\n--- Firmware Version Scan Results ---")
    for hostname, version in versions.items():
        print(f"{hostname}: {version}")

    print("\n--- Summary ---")
    version_counts = Counter(versions.values())
    for version, count in version_counts.items():
        print(f"'{version}': {count} devices")


def main():
    """Main function."""
    run_initial_env_checks()
    parser = argparse.ArgumentParser(description="Dolos fleet rollout tool.")
    parser.add_argument(
        "--csv-file",
        required=True,
        help="Path to the CSV file with the list of devices.",
    )
    parser.add_argument(
        "--set-version",
        help="Set the dolos firmware version for all devices in the CSV.",
    )
    parser.add_argument(
        "--repair",
        action="store_true",
        help="Schedule a repair for all devices in the CSV.",
    )
    parser.add_argument(
        "--repair-only",
        action="store_true",
        help="Only schedule a repair for all devices in the CSV, do not set version.",
    )
    parser.add_argument(
        "--ufs-scan",
        action="store_true",
        help="Scan and report the firmware version of all devices in the CSV.",
    )
    args = parser.parse_args()

    hostnames = get_hostnames_from_csv(args.csv_file)

    if args.ufs_scan:
        scan_firmware_versions(hostnames)
        return

    if not args.repair_only and args.set_version:
        # Version string format is <major>.<minor>.<patch>-<githash>
        # e.g. 0.214.0-c792f03
        version_regex = r"^\d+\.\d+\.\d+-[a-f0-9]{7}$"
        if not re.match(version_regex, args.set_version):
            print(
                f"Error: Invalid firmware version format: '{args.set_version}'.",
                file=sys.stderr,
            )
            print(
                "Expected format is <major>.<minor>.<patch>-<7-char-githash> "
                "(e.g., 0.214.0-c792f03).",
                file=sys.stderr,
            )
            sys.exit(1)
        set_firmware_version(hostnames, args.set_version)

    if args.repair or args.repair_only:
        repair_duts(hostnames)


if __name__ == "__main__":
    main()
