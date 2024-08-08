# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for interacting ssh."""

import logging
import re

import pexpect.exceptions
import pexpect.pxssh


class SSHException(Exception):
    """General exception errors when programming the cable."""


class SshShell:
    """Creates an interactive SSH Shell connection to a remote system."""

    def __init__(self, ssh):
        """Start SSH process."""
        self._ssh = pexpect.pxssh.pxssh(encoding="utf8", codec_errors="replace")
        try:
            self._ssh.login(ssh, username="root")
        except pexpect.exceptions.ExceptionPexpect as err:
            raise SSHException(f"Connection to {ssh} failed") from err

    def connected(self):
        """Returns true if the ssh port is still connected."""
        return self._ssh.isalive()

    def run(self, cmd, timeout=1):
        """Run the command in the ssh process on the remote system.

        Args:
            cmd: Command to run
            timeout: Timeout in seconds for the command to complete

        Returns:
            string: Cleaned response text from ssh
        Raises:
            SSHException: Command has failed to run or return a result
        """
        logging.debug("Command: %r", cmd)
        # Verify the connection is still open
        if not self.connected():
            raise SSHException("Connection closed")

        # Send the command and wait for the response
        try:
            self._ssh.sendline(cmd)
            self._ssh.prompt(timeout=timeout)
        except pexpect.exceptions.ExceptionPexpect as err:
            raise SSHException("Command failed") from err

        # Find the command echo and trim the response
        response = self._ssh.before
        response = re.sub(r"[\r\n]+", "\n", response)
        logging.debug("Response: %r", response)
        cmd += "\n"
        start = response.find(cmd)
        if start == -1:
            raise SSHException("Echo missing")

        response = response[start + len(cmd) :]
        logging.debug("Return: %r", response)
        return response
