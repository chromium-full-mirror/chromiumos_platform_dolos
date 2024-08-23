# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the virtual environment if it's missing
if [ ! -d .venv ]; then
	echo "Building the virtual environment"
	# Verify the virtual environment is installed
	if ! (python -m venv .venv &> /dev/null); then
		echo "Failed to create Python virtual environment."
		return 1
	fi
fi

# Start the environment
source .venv/bin/activate

# Install the requirements
pip install --require-hashes -r requirements.txt -q

# Install the dolos tools if they are missing. These commands take longer
# to run so for now we'll only run once.
if ! [ -x "$(command -v doloscmd)" ]; then
	pip install --no-deps --no-index ./doloscmd
	pip install --no-deps --no-index ./dolosbattery
fi
