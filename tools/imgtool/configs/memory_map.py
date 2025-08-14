# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Configuration file to store memory layout of Dolos board.
"""

KB = 1024
PAD_VALUE = 0xFF

FLASH_TOTAL_SIZE = 128 * KB
BOOT_PARTITION_SIZE = 18 * KB
FIRMWARE_PARTITION_SIZE = FLASH_TOTAL_SIZE - BOOT_PARTITION_SIZE
FIRMWARE_PARTITION_START_ADDR = BOOT_PARTITION_SIZE
