#!/usr/bin/python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Environment checks for Dolos fleet rollout tools."""

from datetime import datetime
import re
import subprocess
import sys


def run_command(command, check=True):
    """Run a command in the shell"""
    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=check,
            text=True,
        )
        return result
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {e}", file=sys.stderr)
        print(f"Stderr: {e.stderr}", file=sys.stderr)
        if check:
            sys.exit(1)
        return e


def run_initial_env_checks():
    """Runs initial checks for shivas and gcertstatus."""
    # Check shivas version
    try:
        shivas_version_output = run_command(["shivas", "version"]).stdout
        match = re.search(
            r"shivas CLI tool: v\d+\.\d+\.\d+\+(\d{8})", shivas_version_output
        )
        if not match:
            print("Error: Could not parse shivas version date.", file=sys.stderr)
            sys.exit(1)
        shivas_date = datetime.strptime(match.group(1), "%Y%m%d").date()
        if shivas_date < datetime.fromisoformat("2025-08-21").date():
            print("Error: shivas version is too old.", file=sys.stderr)
            sys.exit(1)
    except FileNotFoundError:
        print("Error: shivas command not found.", file=sys.stderr)
        sys.exit(1)

    # Check shivas whoami
    if run_command(["shivas", "whoami"], check=False).returncode != 0:
        print("Error: Not logged into shivas.", file=sys.stderr)
        sys.exit(1)

    # Check gcertstatus
    try:
        if (
            subprocess.run(["gcertstatus"], capture_output=True, check=False).returncode
            != 0
        ):
            print("Error: gcertstatus check failed.", file=sys.stderr)
            sys.exit(1)
    except FileNotFoundError:
        print("Error: gcertstatus command not found.", file=sys.stderr)
        sys.exit(1)
