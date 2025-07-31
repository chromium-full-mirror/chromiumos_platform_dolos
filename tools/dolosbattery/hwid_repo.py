# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=too-few-public-methods

"""Parse HWID repo"""

import logging
from multiprocessing import Pool
import re

from dolosbattery.error import DolosBatteryError
import yaml


class HWIDModel:
    """Represents a HWID model and provides methods to parse and process it."""

    all_models = {}

    @classmethod
    def _find_hwidv3_files(cls, path):
        """Navigate the HWID repo and find all HWID v3 files.

        Args:
            path (pathlib.Path): Input path to the base of the repo.

        Returns:
            List[pathlib.Path]: List of files matching the HWID format.
        """
        hwid_files = []
        for f in path.glob("v3/*"):
            # Only files without extensions are valid
            if not f.is_file() or f.suffix != "":
                continue
            hwid_files.append(f)
        return hwid_files

    @classmethod
    def safe_init(cls, file):
        """Safely initialize a HWIDModel object, handling potential errors.

        Args:
            file (pathlib.Path): The path to the HWID file.

        Returns:
            HWIDModel or None: A HWIDModel object if successful, None otherwise.
        """
        try:
            return HWIDModel(file)
        except DolosBatteryError:
            return None

    @classmethod
    def _parse_hwidv3_files(cls, files):
        """Loads all of the HWIDs models in parallel using a pool to improve speed.

        Args:
            files (List[pathlib.Path]): List of HWID model files.

        Returns:
            dict: Mapping of models to their HWIDModel.
        """

        parsed_hwids = []
        with Pool() as p:
            parsed_hwids = p.map(cls.safe_init, files)
        parsed_hwids = [x for x in parsed_hwids if x is not None]
        return {x.model: x for x in parsed_hwids}

    @classmethod
    def build_hwid_map(cls, path):
        """Parse the HWID repo and builds the set of HWIDModels.

        Args:
            path (pathlib.Path): Path to root of HWID repo.

        Returns:
            dict: A dictionary mapping model names to HWIDModel objects.
        """
        files = cls._find_hwidv3_files(path)
        cls.all_models = cls._parse_hwidv3_files(files)
        return cls.all_models

    @classmethod
    def find_model_config(cls, model):
        """Finds the configuration for a specific model.

        Args:
            model (str): The name of the model to find.

        Returns:
            dict or None: The configuration data for the model, or None if not found.
        """
        hwid_model = cls.all_models.get(model)
        if hwid_model is None:
            logging.warning("Model %r is was not found in database", model)
            return None
        return hwid_model.get_config()

    def get_config(self):
        """Generate the config format.

        Returns:
            dict or None: The configuration data, or None if no battery mapping is present.
        """

        # No battery mapping is present
        if self.bat_hash is None:
            return None
        data = {
            "HashHWIDv3": self.bat_hash,
        }
        data |= self.bat_regs
        return data

    def __init__(self, file):
        """Initialize a HWIDModel object.

        Args:
            file (pathlib.Path): The path to the HWID file.
        """
        self.file = file
        self._data = self._load_yaml()
        model = self._data.get("project")
        if model is None:
            model = self._data.get("board")
        self.model = model.lower()
        self._hwid_map = self._parse_hwid(self._data["pattern"])
        self.bat_hash = self._extract_field("battery_field")
        if self.bat_hash:
            self.bat_map = self._parse_battery()
            self.bat_regs = self._extract_regs(self.bat_map)
        else:
            self.bat_map = None
            self.bat_regs = None

    def _load_yaml(self):
        """Load a HWID yaml file.

        Returns:
            dict: HWID model information.
        """

        # Ignore yaml tags
        class YamlLoader(yaml.SafeLoader):
            def parse_tag(self, node):
                # Special handling for regexes
                if node.tag == "!re":
                    return re.compile(node.value)
                return node.value

        YamlLoader.add_constructor(None, YamlLoader.parse_tag)
        txt = self.file.read_text()
        return yaml.load(txt, Loader=YamlLoader)

    def _calc_bitfield(self, fields):
        """Converts between the length based bitfield structure to indexes.

        Example:

        Input:
        [{'mainboard_field': 1}, {'region_field': 2}, {'mainboard_field': 1}]

        Output:
        {'image_id': (0, 1, 2, 3, 4), 'mainboard_field': (5, 8), 'region_field': (6, 7)}

        Args:
            fields (List[dict]): List of bitfields and their lengths.

        Returns:
            dict: Mapping of bitfield names and indexes.

        Raises:
            DolosBatteryError: If an entry has more than one key.
        """

        bitfield_map = {}
        total_bits = 0
        # Prepend the image ID
        all_fields = [{"image_id": 5}] + fields
        for field in all_fields:
            entry = list(field.items())

            # Validation checks
            if len(entry) != 1:
                text = f"File: {self.file}: Each entry must have 1 map: {entry}"
                logging.error(text)
                raise DolosBatteryError(text)

            # Extract the name and size
            name, bitsize = entry[0]

            # Generate the segment. A tuple is used to make later operations
            # easier.
            start = total_bits
            total_bits += bitsize
            bits = tuple(range(start, total_bits))

            # Create the entry if it's missing
            if name not in bitfield_map:
                bitfield_map[name] = bits
            else:
                # Append bits in reverse order
                bitfield_map[name] = bits + bitfield_map[name]
        return bitfield_map

    def _parse_hwid(self, patterns):
        """Creates a mapping between board id and field bit indexes.

        Args:
            patterns (List[dict]): Bitfield pattern structure.

        Returns:
            dict: Map between image ids and one's bitfield combinations.
        """
        hwid_map = {}
        for p in patterns:
            key = tuple(p["image_ids"])
            hwid_map[key] = self._calc_bitfield(p["fields"])
        return hwid_map

    def _extract_field(self, field_name):
        """Find the bitfield pattern for each image ID.

        Finds the mapping between image id and bitfields. Combines the
        results and sorts them by image id.

        Args:
            field_name (str): Field name.

        Returns:
            List[dict] or None: Mapping of image ids to the bits for the specific field.
            Returns None if no valid fields are present.
        """

        # Find the bitfield pattern and all image_ids associated with it
        # This will allow us to consolidate identical maps
        bitfield_to_id = {}

        valid = False

        for ids, bitfield_map in self._hwid_map.items():
            if field_name in bitfield_map:
                valid = True
            field = bitfield_map.get(field_name, tuple())
            prior_ids = bitfield_to_id.get(field, tuple())
            updated_ids = tuple(sorted(set(prior_ids + ids)))
            bitfield_to_id[field] = updated_ids

        if not valid:
            return None

        # Generate a sorted dictionary based on the image ids to ensure
        # a consistent product between runs
        sorted_id_to_bitfield = []
        for bitfield, ids in sorted(bitfield_to_id.items(), key=lambda x: x[1]):
            sorted_id_to_bitfield.append({"ids": list(ids), "bits": list(bitfield)})

        return sorted_id_to_bitfield

    def _get_data(self, path):
        """Helper function to aid in finding data in the structure.

        Helps produce better logs when missing entries are encountered.

        Args:
            path (List[str]): List of getter options.

        Returns:
            Any: Value if found.

        Raises:
            DolosBatteryError: When path does not exist.
        """
        data = self._data
        for p in path:
            if p in data:
                data = data[p]
            else:
                text = f"File: {self.file}: {p} in {path} missing"
                logging.error(text)
                raise DolosBatteryError(text)
        return data

    def _get_battery_indexes(self, battery_field):
        """Parse the battery_field and build a map of index and names.

        The indexes map to the HWID battery_field and tell us which battery
        needs to be used in a model. This performs input validation on the
        configurations and recovers from known variations in formats.

        Example:

        data:
          battery_field:
            0:
              battery: battery_a
            1:
              battery: battery_b
            2:
              battery: battery_c

        Result:
          {0: battery_a, 1: battery_b, 2:battery_c}

        Args:
            battery_field (dict): The battery field data.

        Returns:
            dict: Mapping between index and name.

        Raises:
            DolosBatteryError: Failed to repair mapping.
        """
        bat_map = {}
        for index, field in battery_field.items():
            if len(field) != 1:
                text = f"File: {self.file}: {field} is invalid"
                logging.error(text)
                raise DolosBatteryError(text)

            battery_name = field["battery"]
            if isinstance(battery_name, list):
                if len(battery_name) != 1:
                    text = f"File: {self.file}: {battery_name} should have 1 entry"
                    logging.error(text)
                    raise DolosBatteryError(text)

                battery_name = battery_name[0]
            bat_map[index] = battery_name
        return bat_map

    def _parse_battery_values(self, battery_components):
        """Parse the battery values to find the patterns for each name.

        This tells us which battery registers should be present in each
        battery name.

        Example:

        data:
          battery:
            items:
              battery_a:
                values:
                  manufacturer: MAN_A
                  model_name: MODEL_A
              battery_b:
                values:
                  manufacturer: MAN_B
              battery_c:
                values:
                  manufacturer: MAN_C
                  model_name: MOD_C
                  technology: Lion


        Result:
            {
                battery_a: {label: battery_a, manufacturer:MAN_A, model_name:MODEL_A},
                battery_b: {label: battery_b, manufacturer: MAN_B},
                battery_c: {label: battery_c, manufacturer:MAN_C, model_name:MOD_C, technology: Lion},
            }

        Args:
            battery_components (dict): Battery components structure.

        Returns:
            dict: Maps between name and registers.
        """
        battery_values_map = {}
        for name, field in battery_components.items():
            values = field["values"]
            if values is None:
                values = {}
            battery_values_map[name] = values
        return battery_values_map

    def _parse_battery(self):
        """Build a mapping between the index and register values.

        Returns:
            dict: Mapping between the battery indexes and values.

        Raises:
            DolosBatteryError: If a battery name is missing from the components.
        """

        # Build map for inex to name
        battery_index_fields = self._get_data(["encoded_fields", "battery_field"])
        index_name_map = self._get_battery_indexes(battery_index_fields)
        battery_components = self._get_data(["components", "battery", "items"])
        name_regs_map = self._parse_battery_values(battery_components)

        # Build mapping between index to registers
        index_val_map = {}
        for index, name in index_name_map.items():
            if name is None:
                # Several battery models are null
                continue
            regs = name_regs_map.get(name)
            if regs is None:
                text = f"File: {self.file}: {index}:{name} missing"
                logging.error(text)
                raise DolosBatteryError(text)
            index_val_map[index] = regs
        return index_val_map

    def _extract_regs(self, index_val_map):
        """Converts the index value map to registers.

        Args:
            index_val_map (dict): Mapping of index to values.

        Returns:
            dict: Mapping of registers to their values.

        Raises:
            DolosBatteryError: If an unknown battery key is encountered.
        """

        # Mapping of known keys to their registers. Fields with None are ignored.
        known_keys = {
            "model_name": "SB_DEVICE_NAME",
            "manufacturer": "SB_MANUFACTURER_NAME",
            "chemistry": "SB_DEVICE_CHEMISTRY",
            "technology": "SB_DEVICE_CHEMISTRY",
            "charge_full_design": None,
            "compact_str": None,
            "device_path": None,
            "type": None,
            "label": None,
        }

        def reverse_regex(reg_pat):
            """Reverses simple regex patterns to find their match.

            We need to convert several simple regex patterns into valid matches
            for the battery registers. This handles patterns seen so far.

            Args:
                reg_pat (re.Pattern): Regex pattern we want to match

            Returns:
                str: A string which should match the pattern
            Raises:
                DolosBatteryError: If reversal failed
            """
            val = reg_pat.pattern
            # Replace '.*' with ''
            val = val.replace(".*", "")
            # Replace wildcards for example `\d` with 0
            val = re.sub(r"\\d", r"0", val)
            # Bracket expressions like [abc] or [a-z] can be replaced with the first character
            val = re.sub(r"\[(.)[^\]]*]", r"\1", val)
            # Replace subexpressions with the first chunk (abc|123) => abc
            val = re.sub(r"\(([^|)]*)[^)]*?\)", r"\1", val)
            if not reg_pat.match(val):
                text = (
                    f"File: {self.file}: Failed to reverse the regex: {reg_pat.pattern}"
                )
                logging.error(text)
                raise DolosBatteryError(text)
            return val

        bat_regs = {}

        for index, values in index_val_map.items():
            id_key = f"HWIDv3-{index}"

            for k, v in values.items():

                if k not in known_keys:
                    text = f"File: {self.file}: Unknown battery key: {k}"
                    logging.error(text)
                    raise DolosBatteryError(text)

                dest_reg = known_keys[k]

                # Some are ignored
                if dest_reg is None:
                    continue

                # Reverse any regex patterns
                if isinstance(v, re.Pattern):
                    reverse_regex(v)

                # Convert strings to binary
                if isinstance(v, str):
                    v = [ord(x) for x in v]

                # Store the data
                if dest_reg not in bat_regs:
                    bat_regs[dest_reg] = {}
                bat_regs[dest_reg][id_key] = v
        return bat_regs
