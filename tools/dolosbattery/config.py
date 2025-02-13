# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a mechanism to load the configs and map the HWID correctly."""

import logging
import yaml

from dolosbattery import download
from dolosbattery.hwid import HWIDV3, HWIDException


class ConfigException(Exception):
    """General exception for HWID related errors."""


class Config:
    """Manages the battery config and HWID."""

    def __init__(self):
        self.table = None
        self.hwid = None

    def set_hwid(self, hwid):
        """Set the HWID.

        Args:
            hwid (string): HWID string with all components.

        Raises:
            ConfigException: Invalid HWID
        """
        try:
            self.hwid = HWIDV3(hwid)
        except HWIDException as err:
            raise ConfigException(f"Failed to update HWID: {hwid}") from err

    def load_config_file(self, file):
        """Loads the yaml describing the battery cable configuration.

        Args:
            file (pathlib.Path): File to load

        Raises:
            ConfigException: Error reading or parsing the config
        """
        logging.info("Loading yaml: %r", file)
        try:
            txt = file.read_text()
        except FileNotFoundError as err:
            raise ConfigException(f"Unable to read file: {file}") from err
        self._import_yaml(txt)

    def load_latest_config(self, model):
        """Loads the yaml describing the battery cable configuration.

        Args:
            model (string): Model
        Raises:
            ConfigException: Error fetching or parsing the config
        """
        try:
            config_list = download.load_cable_list(model)
            latest_path = download.find_latest_config(config_list)
            txt = download.load_config_file(latest_path)
        except download.DownloadException as err:
            raise ConfigException(f"Unable to fetch config: {model}") from err

        self._import_yaml(txt)

    def _import_yaml(self, txt):
        """Import the yaml and update the table.

        Args:
            txt (string): Text data

        Raises:
            ConfigException: Parsing the yaml data
        """
        try:
            self.table = yaml.safe_load(txt)
        except (yaml.scanner.ScannerError, yaml.YAMLError) as err:
            raise ConfigException("Unable to parse yaml file") from err

    def _get_battery_index(self):
        """Calculate the battery index.

        Returns:
            int Battery index in HWID if it is present.
                0 is returned if HWID or HWID patterns are missing.

        Raises:
            ConfigException: HWID does not match the known patterns
        """
        hwid_patterns = self.hwid_patterns
        if self.hwid is None or hwid_patterns is None:
            return 0
        try:
            return self.hwid.read_bitfield(hwid_patterns)
        except HWIDException as err:
            raise ConfigException("Failed to extract HWID from bit field") from err

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
            ConfigException: Error when parsing the HWID or finding required entries.
        """

        battery_index = self._get_battery_index()
        battery_key = f"HWIDv3-{battery_index}"

        parsed_table = {}

        for key, val in self.table.items():

            if isinstance(val, dict):
                # Multi-battery register
                if battery_key not in val:
                    raise ConfigException(
                        f"Config key {key} does not support index {battery_index}"
                    )
                parsed_table[key] = val[battery_key]

            else:
                # Simple battery register
                parsed_table[key] = val

        return parsed_table
