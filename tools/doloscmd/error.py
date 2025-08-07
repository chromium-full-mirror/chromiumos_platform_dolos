# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common error for DolosConsole."""


class DolosConsoleError(Exception):
    """General exception thrown when a error occurs dealing with the console."""

class DolosConsoleNoEchoError(DolosConsoleError):
    """Thrown when a command to the console fails to echo back the commands."""