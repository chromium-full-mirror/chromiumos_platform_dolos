# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a mechanism to load the configs and map the HWID correctly."""

import logging
from pathlib import Path

from doloscmd import download
from doloscmd.error import DolosConsoleError
from doloscmd.hwid import HWIDV3
import yaml


class Config:
    """Manages the battery config and HWID."""

    def __init__(self, model_hwid=None):
        """Creates a new Config instance

        Args:
            model_hwid (string): Optional config model or full hwid

        Raises:
            DolosConsoleError: HWID is invalid
        """
        self.table = None
        self.model = None
        self.hwid = None

        if model_hwid:
            if len(model_hwid.split()) == 1:
                self.model = model_hwid
            else:
                self.hwid = HWIDV3(model_hwid)
                self.model = self.hwid.model

    def load_config(self, file=None):
        """Loads the yaml describing the battery cable configuration.

        Args:
            file (string): Optional local path to file which can be used either
                           instead of model or bypass the network config.
        Raises:
            DolosConsoleError: Error fetching or parsing the config
        """
        txt = None
        if file:
            txt = self._load_local_config(Path(file))
        elif self.model:
            txt = self._load_network_config()
        else:
            raise DolosConsoleError("No config specified")

        self._import_yaml(txt)

    def _load_local_config(self, file):
        """Loads the yaml describing the battery cable configuration.

        Args:
            file (pathlib.Path): File to load

        Returns:
            txt: Config file text

        Raises:
            DolosConsoleError: Error reading the file
        """
        logging.info("Loading yaml: %r", file)
        try:
            return file.read_text()
        except FileNotFoundError as err:
            raise DolosConsoleError(f"Unable to read file: {file}") from err

    def _load_network_config(self):
        """Loads the latest config from the network storage

        Returns:
            txt: Config file text

        Raises:
            DolosConsoleError: Error fetching the config
        """
        config_list = download.load_cable_list(self.model)
        latest_path = download.find_latest_config(config_list)
        return download.load_config_file(latest_path)

    def _import_yaml(self, txt):
        """Import the yaml and update the table.

        Args:
            txt (string): Text data

        Raises:
            DolosConsoleError: Parsing the yaml data
        """
        try:
            self.table = yaml.safe_load(txt)
        except (yaml.scanner.ScannerError, yaml.YAMLError) as err:
            raise DolosConsoleError("Unable to parse yaml file") from err

    def _get_battery_index(self):
        """Calculate the battery index.

        Returns:
            int Battery index in HWID if it is present.
                None is returned if HWID or HWID patterns are missing.

        Raises:
            DolosConsoleError: HWID does not match the known patterns
        """
        hwid_patterns = self.hwid_patterns
        if self.hwid is None or hwid_patterns is None:
            return None
        try:
            return self.hwid.read_bitfield(hwid_patterns)
        except DolosConsoleError as err:
            raise DolosConsoleError("Failed to extract HWID from bit field") from err

    @property
    def hwid_patterns(self):
        """Return the HWID Hash Bitpatterns."""
        return self.table.get("HashHWIDv3")

    def extract_table(self):
        """Parse the HWID and config table to extract the battery table.

        Parse the loaded config table and extract the battery fields.
        When a key:val pair has no hash mapping, no transformations are needed.
        If a key:val pair is mapped to multiple batteries, it will identify
        the correct battery and replace it in the returned table.

        Raises:
            DolosConsoleError: Error when parsing the HWID or finding required entries.
        """

        battery_key = None
        battery_index = self._get_battery_index()
        if battery_index is not None:
            battery_key = f"HWIDv3-{battery_index}"

        parsed_table = {}

        for key, val in self.table.items():

            if isinstance(val, dict):
                # Multi-battery register

                # Handle the situation where we have no index
                if battery_key is None:
                    key_map = [(int(x.split("-")[-1]), x) for x in val.keys()]
                    lowest_key = sorted(key_map)[0][1]
                    parsed_table[key] = val[lowest_key]
                elif battery_key in val:
                    parsed_table[key] = val[battery_key]
                else:
                    raise DolosConsoleError(
                        f"Key:{key} does not support index {battery_index}"
                    )

            else:
                # Simple battery register
                parsed_table[key] = val

        return parsed_table
