# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib

import pytest


from dolosbattery.hwid_repo import HWIDModel
from dolosbattery.error import DolosBatteryError


class TestHWIDModel:
    def _load_hwid(self, data_path):
        path = pathlib.Path(__file__).parent / "../data" / data_path
        return HWIDModel(path)

    def test_import_model(self):
        """Verify we can import a mock HWID correctly"""

        hwid_model = self._load_hwid("hwid_test.yaml")

        EXP_BAT_HASH = [
            {"ids": [0, 1], "bits": [16, 15]},
            {"ids": [3], "bits": [17, 15, 16]},
        ]

        EXP_BAT_MAP = {
            0: {},
            1: {
                "manufacturer": "BAT_REGEX",
                "model_name": "BAT[0-9]_TEST.*",
                "technology": "Li-poly",
            },
        }

        EXP_BAT_REGS = {
            "SB_MANUFACTURER_NAME": {"HWIDv3-1": [66, 65, 84, 95, 82, 69, 71, 69, 88]},
            "SB_DEVICE_NAME": {
                "HWIDv3-1": [66, 65, 84, 91, 48, 45, 57, 93, 95, 84, 69, 83, 84, 46, 42]
            },
            "SB_DEVICE_CHEMISTRY": {"HWIDv3-1": [76, 105, 45, 112, 111, 108, 121]},
        }

        EXP_CFG = {
            "HashHWIDv3": [
                {"ids": [0, 1], "bits": [16, 15]},
                {"ids": [3], "bits": [17, 15, 16]},
            ],
            "SB_MANUFACTURER_NAME": {"HWIDv3-1": [66, 65, 84, 95, 82, 69, 71, 69, 88]},
            "SB_DEVICE_NAME": {
                "HWIDv3-1": [66, 65, 84, 91, 48, 45, 57, 93, 95, 84, 69, 83, 84, 46, 42]
            },
            "SB_DEVICE_CHEMISTRY": {"HWIDv3-1": [76, 105, 45, 112, 111, 108, 121]},
        }

        assert hwid_model.model == "test"
        assert hwid_model.bat_hash == EXP_BAT_HASH
        assert hwid_model.bat_map == EXP_BAT_MAP
        assert hwid_model.bat_regs == EXP_BAT_REGS
        assert hwid_model.get_config() == EXP_CFG

    def test_joined_fields(self):
        """Verify identifical patterns are joined correctly."""

        model = self._load_hwid("hwid_test.yaml")

        EXP_A_FIELD = [{"ids": [0, 1, 3], "bits": [5, 6, 7, 8, 9]}]
        assert model._extract_field("a_field"), EXP_A_FIELD

    def test_bad_config(self):
        """Verify it detect invalid configs."""

        with pytest.raises(DolosBatteryError):
            model = self._load_hwid("hwid_missing_comp.yaml")
