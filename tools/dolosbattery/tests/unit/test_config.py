# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pytest

from dolosbattery.config import Config, ConfigException


class TestConfig:

    HASH_HWID = [
        {"ids": [0], "bits": [11, 12]},
        {"ids": [1, 2], "bits": [14, 17]},
        {"ids": [15], "bits": []},
    ]

    DEVICE_0 = [ord(x) for x in "BAT"]
    DEVICE_1 = [ord(x) for x in "TEST"]
    DEVICE_2 = [ord(x) for x in "NAMES"]

    SERIAL_0 = 123
    SERIAL_1 = 456
    SERIAL_2 = 789

    SB_DESIGN_VOLTAGE = 12000

    SB_MANUFACTURER_NAME = [ord(x) for x in "MANUF"]

    SB_DEVICE_NAME_MAP = {
        "HWIDv3-0": DEVICE_0,
        "HWIDv3-1": DEVICE_1,
        "HWIDv3-2": DEVICE_2,
    }

    SB_SERIAL_NUMBER_MAP = {
        "HWIDv3-0": SERIAL_0,
        "HWIDv3-1": SERIAL_1,
        "HWIDv3-2": SERIAL_2,
    }

    CONFIG_COMPLEX = {
        "Polarity": "high",
        "HashHWIDv3": HASH_HWID,
        "SB_DESIGN_VOLTAGE": SB_DESIGN_VOLTAGE,
        "SB_MANUFACTURER_NAME": SB_MANUFACTURER_NAME,
        "SB_DEVICE_NAME": SB_DEVICE_NAME_MAP,
        "SB_SERIAL_NUMBER": SB_SERIAL_NUMBER_MAP,
    }

    CONFIG_0 = {
        "Polarity": "high",
        "SB_DESIGN_VOLTAGE": SB_DESIGN_VOLTAGE,
        "SB_MANUFACTURER_NAME": SB_MANUFACTURER_NAME,
        "SB_DEVICE_NAME": DEVICE_0,
        "SB_SERIAL_NUMBER": SERIAL_0,
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

    def _assert_req_map(self, required, result):
        """Verify all required members are in the resulting map."""
        for key, exp_val in required.items():
            assert key in result
            act_val = result[key]
            assert exp_val == act_val

    def test_simple_config(self):
        """Verify a simple non-hash HWID works."""
        config = Config()
        config.table = self.CONFIG_0

        # The extraction should be identical
        self._assert_req_map(self.CONFIG_0, config.extract_table())

        # There should be no differences when we set the HWID
        config.set_hwid("MODEL P2DD2AB9O")
        self._assert_req_map(self.CONFIG_0, config.extract_table())

    def test_complex_config(self):
        """Verify a simple non-hash HWID works."""
        config = Config()
        config.table = self.CONFIG_COMPLEX

        # The extraction should return CONFIG_0 when the HWID is not set
        self._assert_req_map(self.CONFIG_0, config.extract_table())

        # HWID revision_id=0 and battery=1 => CONFIG_1
        config.set_hwid("MODEL A2BA2AB2G")
        self._assert_req_map(self.CONFIG_1, config.extract_table())

        # HWID revision_id=2 and battery=2 => CONFIG_2
        config.set_hwid("MODEL C2DI2AB5A")
        self._assert_req_map(self.CONFIG_2, config.extract_table())

        # HWID revision_id=15 => CONFIG_0
        config.set_hwid("MODEL P2DD2AB9O")
        self._assert_req_map(self.CONFIG_0, config.extract_table())

        # HWID revision_id=0 and battery=3 => Invalid battery index
        config.set_hwid("MODEL A2DB2AB4I")
        with pytest.raises(ConfigException):
            config.extract_table()

        # HWID revision_id=4 => Invalid revision_id
        config.set_hwid("MODEL D2DI2AB9O")
        with pytest.raises(ConfigException):
            config.extract_table()

    def test_find_latest_cfg(self, mock_storage_response):
        """Verify we find the latest config."""

        config = Config()

        config.load_latest_config("model_a")
        assert config.table["FileName"] == "model_a_v1.yaml"

        config.load_latest_config("model_b")
        assert config.table["FileName"] == "model_b_v2.yaml"

        # Verify invalid cases will not load
        with pytest.raises(ConfigException):
            config.load_latest_config("model_invalid")
