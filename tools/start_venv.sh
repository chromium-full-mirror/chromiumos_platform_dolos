#!/usr/bin/env bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the virtual environment if it's missing
if [ ! -d .venv ]; then
	echo "Building the virtual environment"
	# Verify the virtual environment is installed
	if ! (python3 -m venv .venv &> /dev/null); then
		echo "Failed to create Python virtual environment."
		return 1
	fi
fi

# Start the environment
# shellcheck source=/dev/null
source .venv/bin/activate

# Install the requirements
pip3 install --require-hashes -r requirements.txt -q --break-system-packages

# Install the dolos tools if they are missing. These commands take longer
# to run so for now we'll only run once.
if ! [ -x "$(command -v doloscmd)" ]; then
	pip3 install --no-deps --no-index ./doloscmd ./dolosbattery --break-system-packages --no-build-isolation
fi
