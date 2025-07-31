# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a mechanism to operate on a config directory."""

import logging
from multiprocessing import Pool
import re

from dolosbattery.error import DolosBatteryError
from dolosbattery.hwid_repo import HWIDModel
import yaml


class Config:
    """
    Represents a battery configuration file and provides methods to parse and process it.
    """

    @classmethod
    def _find_cfg_files(cls, path):
        """Search for all yaml battery config files in a given directory.

        Args:
            path (pathlib.Path): Target directory

        Returns:
            List(pathlib.Path): List of configs found
        """
        cfg_files = []
        for f in path.glob("*.yaml"):
            if not f.is_file():
                continue
            cfg_files.append(f)
        return cfg_files

    @classmethod
    def safe_init(cls, args):
        """Safely initialize a Config object, handling potential errors.

        Args:
            args (tuple): A tuple containing the input file path and output file path.

        Returns:
            Config or None: A Config object if successful, None otherwise.
        """
        input_file, output_file = args
        try:
            return Config(input_file, output_file)
        except DolosBatteryError as err:
            logging.error("File %r: Error %r", input_file, err)
            return None

    @classmethod
    def parse_cfg_files(cls, input_dir, output_dir):
        """Parse multiple configuration files in parallel.

        Args:
            input_dir (pathlib.Path): The directory containing the input configuration files.
            output_dir (pathlib.Path): The directory where the processed configuration files will be saved.
        """
        files = cls._find_cfg_files(input_dir)

        if not output_dir.exists():
            output_dir.mkdir()

        args = [(x, output_dir / x.name) for x in files]
        with Pool() as p:
            p.map(cls.safe_init, args)

    def __init__(self, in_file, out_file):
        """Initialize a Config object.

        Args:
            in_file (pathlib.Path): The path to the input configuration file.
            out_file (pathlib.Path): The path to the output configuration file.
        """
        self.in_file = in_file
        self.out_file = out_file

        model = self.in_file.name

        model = re.match(r"(?P<model>.*?)(_v\d+)?\.yaml", model).group("model")

        self.model = model

        self.data = {"Model": self.model}
        self.data |= self._load_yaml()

        hwid_model_data = HWIDModel.find_model_config(self.model)
        if hwid_model_data:
            self.data |= hwid_model_data
        self.out_file.write_text(yaml.dump(self.data, default_flow_style=None))

    def _load_yaml(self):
        """Load the YAML data from the input file.

        Returns:
            dict: The loaded YAML data.
        """
        txt = self.in_file.read_text()
        return yaml.safe_load(txt)
