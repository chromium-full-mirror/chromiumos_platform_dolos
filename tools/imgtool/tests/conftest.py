# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These imports are necessary for pytest dependency injection.
# pylint: disable=unused-import
# pylint: disable=import-error
# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring

import os
import sys

import pytest


def pytest_configure(config):
    print("Run conftest")
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
