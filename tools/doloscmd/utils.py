# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions shared between doloscmd and console_lib."""

import sys


def debug_print(message):
    """Print out all debug messages to stderr.

        Prefix all debug messages with DEBUG >.  As to be easy to parse out
        if necessary
    `

        Args:
            message (string): debug message to print.
    """
    print(f"DEBUG > {message}", file=sys.stderr)
