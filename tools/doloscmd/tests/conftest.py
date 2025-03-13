# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These imports are necessary for pytest dependency injection.
# pylint: disable=unused-import
# pylint: disable=import-error
# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=missing-class-docstring

from doloscmd.tests.fixtures.mock_dolos_console import mock_dolos_console
from doloscmd.tests.fixtures.mock_dolos_host import mock_dolos_host
from doloscmd.tests.fixtures.mock_dolos_host_with_consoles import (
    mock_host_with_multiple_dolos,
)
from doloscmd.tests.fixtures.mock_dolos_host_with_consoles import (
    mock_host_with_one_dolos,
)
from doloscmd.tests.fixtures.mock_requests import mock_storage_response
import pytest
