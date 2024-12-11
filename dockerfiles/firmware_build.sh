#!/bin/bash
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ! This file is used as entrypoint in the Dockerfile.firmware container !
# ! Do not execute manually or outside of the docker container           !

# shellcheck disable=SC1091

set -e

if [ ! -d /repo ] || [ ! -d /zephyr ]; then
	echo "Do not execute this file outside of docker container!"
	exit 1
fi

export HOME=/tmp
git config --global --add safe.directory /zephyr/zephyr
source /zephyr/zephyr/zephyr-env.sh
cd /repo/firmware-zephyr
./src/build.sh
tiarmhex /repo/firmware-zephyr/build/zephyr/zephyr.elf --ti_txt --diag_wrap=off -o /repo/firmware-zephyr/build/zephyr/zephyr.txt
