#!/usr/bin/env python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build the Dolos python code into an distribution package."""

from setuptools import setup


setup(
    name="dolosbattery",
    version="0.1",
    maintainer="chromium os",
    maintainer_email="chromium-os-dev@chromium.org",
    license="Chromium",
    package_dir={"dolosbattery": "../dolosbattery"},
    packages=[
        "dolosbattery",
    ],
    entry_points={
        "console_scripts": [
            "dolos_load_battery=dolosbattery.load_battery:main",
            "dolos_process_configs=dolosbattery.process_configs:main",
        ]
    },
    description="Dolos command tools for reading batteries and programming cables.",
)
