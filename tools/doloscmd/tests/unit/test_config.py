# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from doloscmd.config import Config
from doloscmd.config import DolosConsoleError
import pytest


class TestConfig:

    HASH_HWID = [
        {"ids": [0], "bits": [11, 12]},
        {"ids": [1, 2], "bits": [14, 17]},
        {"ids": [15], "bits": []},
    ]

    DEVICE_1 = [ord(x) for x in "BAT"]
    DEVICE_2 = [ord(x) for x in "TEST"]
    DEVICE_3 = [ord(x) for x in "NAMES"]

    SERIAL_1 = 123
    SERIAL_2 = 456
    SERIAL_3 = 789

    SB_DESIGN_VOLTAGE = 12000

    SB_MANUFACTURER_NAME = [ord(x) for x in "MANUF"]

    SB_DEVICE_NAME_MAP = {
        "HWIDv3-1": DEVICE_1,
        "HWIDv3-2": DEVICE_2,
        "HWIDv3-3": DEVICE_3,
    }

    SB_SERIAL_NUMBER_MAP = {
        "HWIDv3-1": SERIAL_1,
        "HWIDv3-2": SERIAL_2,
        "HWIDv3-3": SERIAL_3,
    }

    CONFIG_COMPLEX = {
        "Polarity": "high",
        "HashHWIDv3": HASH_HWID,
        "SB_DESIGN_VOLTAGE": SB_DESIGN_VOLTAGE,
        "SB_MANUFACTURER_NAME": SB_MANUFACTURER_NAME,
        "SB_DEVICE_NAME": SB_DEVICE_NAME_MAP,
        "SB_SERIAL_NUMBER": SB_SERIAL_NUMBER_MAP,
    }

    CONFIG_1 = {
        "Polarity": "high",
        "SB_DESIGN_VOLTAGE": SB_DESIGN_VOLTAGE,
        "SB_MANUFACTURER_NAME": SB_MANUFACTURER_NAME,
        "SB_DEVICE_NAME": DEVICE_1,
        "SB_SERIAL_NUMBER": SERIAL_1,
    }

    CONFIG_2 = {
        "Polarity": "high",
        "SB_DESIGN_VOLTAGE": SB_DESIGN_VOLTAGE,
        "SB_MANUFACTURER_NAME": SB_MANUFACTURER_NAME,
        "SB_DEVICE_NAME": DEVICE_2,
        "SB_SERIAL_NUMBER": SERIAL_2,
    }

    CONFIG_3 = {
        "Polarity": "high",
        "SB_DESIGN_VOLTAGE": SB_DESIGN_VOLTAGE,
        "SB_MANUFACTURER_NAME": SB_MANUFACTURER_NAME,
        "SB_DEVICE_NAME": DEVICE_3,
        "SB_SERIAL_NUMBER": SERIAL_3,
    }

    def _assert_req_map(self, required, result):
        """Verify all required members are in the resulting map."""
        for key, exp_val in required.items():
            assert key in result
            act_val = result[key]
            assert exp_val == act_val

    def _create_table(self, table, model_hwid=None):
        """Create the config, bypassing the loading utilities."""
        cfg = Config(model_hwid)
        cfg.table = table
        return cfg.extract_table()

    def test_simple_config(self):
        """Verify a simple non-hash HWID works."""
        table = self._create_table(self.CONFIG_1)

        # The extraction should be identical
        self._assert_req_map(self.CONFIG_1, table)

        # There should be no differences when we set the HWID
        table = self._create_table(self.CONFIG_1, "MODEL P2DD2AB9O")
        self._assert_req_map(self.CONFIG_1, table)

    def test_complex_config(self):
        """Verify a simple non-hash HWID works."""
        table = self._create_table(self.CONFIG_COMPLEX)

        # The extraction should return CONFIG_1 when the HWID is not set
        self._assert_req_map(self.CONFIG_1, table)

        # HWID revision_id=0 and battery=1 => CONFIG_1
        table = self._create_table(self.CONFIG_COMPLEX, "MODEL A2BA2AB2G")
        self._assert_req_map(self.CONFIG_1, table)

        # HWID revision_id=2 and battery=2 => CONFIG_2
        table = self._create_table(self.CONFIG_COMPLEX, "MODEL C2DI2AB5A")
        self._assert_req_map(self.CONFIG_2, table)

        # HWID revision_id=0 and battery=3 => CONFIG_3
        table = self._create_table(self.CONFIG_COMPLEX, "MODEL A2DB2AB4I")
        self._assert_req_map(self.CONFIG_3, table)

        # HWID revision_id=15 => CONFIG_1
        table = self._create_table(self.CONFIG_COMPLEX, "MODEL P2DD2AB9O")
        self._assert_req_map(self.CONFIG_1, table)

        # HWID revision_id=4 => Invalid revision_id
        with pytest.raises(DolosConsoleError):
            table = self._create_table(self.CONFIG_COMPLEX, "MODEL D2DI2AB9O")

    def test_find_latest_cfg(self, mock_storage_response):
        """Verify we find the latest config."""

        config = Config("model_a")

        config.load_config()
        assert config.table["FileName"] == "model_a_v1.yaml"

        config = Config("MODEL_B")

        config.load_config()
        assert config.table["FileName"] == "model_b_v2.yaml"

        # Verify invalid cases will not load
        with pytest.raises(DolosConsoleError):
            config = Config("model_invalid")
            config.load_config()
