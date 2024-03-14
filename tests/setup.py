"""Setup file for factory tests"""

# Lint as: python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import setuptools


setuptools.setup(
    name="dolos-factory-tests",
    version="1.0",
    include_package_data=True,
    packages=["dolos_factory_tests"],
    install_requires=[
        "pyserial",
        "pytest",
    ],
    entry_points={
        "console_scripts": [
            "run_dolos_tests = dolos_factory_tests.run_tests:main",
        ]
    },
    package_data={"dolos_factory_tests": ["fw-updater", "dolos.txt"]},
)
