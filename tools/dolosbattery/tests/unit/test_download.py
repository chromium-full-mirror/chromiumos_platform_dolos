# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pytest

from dolosbattery import download


class TestDownloader:
    def test_find_latest(self):
        """Verify we locate the latest config."""

        cfg_paths = [
            "cable_eeprom/model/model_v1.yaml",
            "cable_eeprom/model/model_v2.yaml",
            "cable_eeprom/model/model_v10.yaml",
            "cable_eeprom/model/model_v3.yaml",
            "cable_eeprom/model/model_v4.yaml",
        ]

        path = download.find_latest_config(cfg_paths)

        assert path == "cable_eeprom/model/model_v10.yaml"

    def test_find_cfg(self, mock_storage_response):
        """Verify we can fetch file lists and model filtering works."""
        cfg_list = download.load_cable_list("model_a")
        assert len(cfg_list) == 2

        cfg_list = download.load_cable_list("model_missing")
        assert len(cfg_list) == 0

    def test_find_cfg(self, mock_storage_response):
        """Verify we find the latest config."""
        cfg = download.load_latest_config("model_a")
        assert cfg["FileName"] == "model_a_v2.yaml"
