#!/bin/bash

# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

export SERVO_SERIAL_MAIN=$1
export SERVO_SERIAL_POWER=$2
export TARGET_FW_VERSION=$3

if [ -z "$SERVO_SERIAL_MAIN" ] || [ -z "$SERVO_SERIAL_POWER" ]; then
    echo "[ERROR]: Both SERVO_SERIAL_MAIN and SERVO_SERIAL_POWER must be provided."
    exit 1
fi

if [ -z "$TARGET_FW_VERSION" ]; then
    echo "[ERROR]: Must provide a target firmware version."
    exit 1
fi

docker compose up --build --exit-code-from tester
