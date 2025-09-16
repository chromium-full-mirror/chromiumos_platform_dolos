#!/bin/bash
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ! This file is used as entrypoint in the Dockerfile.firmware container !
# ! Do not execute manually or outside of the docker container           !

# shellcheck disable=SC1091

IMGTOOL="/usr/local/imgtool/imgtool.py"
CONVERTER="/usr/local/bin/converter.py"
FIRMWARE_BINARY="/repo/firmware-zephyr/build/zephyr/zephyr.bin"
LEGACY_FINAL="/repo/firmware-zephyr/build/zephyr/zephyr.txt"
FIRMWARE_FINAL="/repo/firmware-zephyr/build/zephyr/firmware.txt"
BOOTLOADER_BINARY="/repo/bootloader-zephyr/build/zephyr/zephyr.bin"
BOOTLOADER_FINAL="/repo/bootloader-zephyr/build/zephyr/boot.txt"
COMBINED_FINAL="/repo/firmware-zephyr/build/zephyr/combined.txt"
TEMP_PADDED="/tmp/zephyr.padded.bin"
FIRMWARE_START_ADDR=$(python3 $IMGTOOL get -x FIRMWARE_PARTITION_START_ADDR)

MAX_USAGE=90 # Expressed as percentage
FLASH_SIZE=$(python3 $IMGTOOL get FLASH_TOTAL_SIZE)
BOOT_PARTITION_SIZE=$(python3 $IMGTOOL get BOOT_PARTITION_SIZE)
FIRMWARE_PARTITION_SIZE=$(python3 $IMGTOOL get FIRMWARE_PARTITION_SIZE)

# Arg 1: binary file path
# Arg 2: max partition size
function check_size {
	local binary_size
	local max_allowable_size
	binary_size=$(wc -c "$1" | awk '{print $1}')
	max_allowable_size=$(((MAX_USAGE * $2) / 100))
	if (( binary_size > max_allowable_size )); then
		echo "ERROR: Binary $1 exceeds maximum occupation threshold of $MAX_USAGE%"
		exit 1
	fi
}

function build_legacy {
	cd /repo/firmware-zephyr

	if [[ -f "app.overlay" ]]; then
		echo "Found overlay file. Removing from legacy build"
		rm app.overlay
	fi

	# TODO(b/438110087): Remove rm call when legacy no longer supported
	if [[ -d "build" ]]; then
                rm -r build
        fi

	./src/version.sh
	west build -b dolos
	check_size $FIRMWARE_BINARY "$FLASH_SIZE"

	python3 $IMGTOOL pad --legacy $FIRMWARE_BINARY $TEMP_PADDED
	python3 $CONVERTER $TEMP_PADDED $LEGACY_FINAL

	rm $TEMP_PADDED
}

function build_bootloader {
	cd /repo/bootloader-zephyr

        # Source to retain BOOTLOADER_VERSION_STRING env variable
        source ./src/version.sh

	west build -b dolos
	check_size $BOOTLOADER_BINARY "$BOOT_PARTITION_SIZE"

	python3 $IMGTOOL pad --boot $BOOTLOADER_BINARY $TEMP_PADDED
        if [[ "$DEV_MODE" == "enabled" ]]; then
                python3 $IMGTOOL append-ro $TEMP_PADDED "$BOOTLOADER_VERSION_STRING" --dev
        else
                python3 $IMGTOOL append-ro $TEMP_PADDED "$BOOTLOADER_VERSION_STRING"
        fi
	python3 $CONVERTER $TEMP_PADDED $BOOTLOADER_FINAL

	rm $TEMP_PADDED
}

function build_firmware {
	cd /repo/firmware-zephyr

	rm -r build

	# TODO(b/438110087): Move first 3 options to prj.conf and remove BOOTLOADER_ENABLED symbol
	# And once legacy option is removed, rm -r build calls can be removed
	./src/version.sh
	west build -b dolos -- -DCONFIG_USE_DT_CODE_PARTITION=y -DCONFIG_FLASH=y -DCONFIG_FLASH_MAP=y -DCONFIG_BOOTLOADER_ENABLED=y
	check_size $FIRMWARE_BINARY "$FIRMWARE_PARTITION_SIZE"

	python3 $IMGTOOL pad --firmware $FIRMWARE_BINARY $TEMP_PADDED
        python3 $IMGTOOL crc-fw $TEMP_PADDED
	python3 $CONVERTER --section_start "$FIRMWARE_START_ADDR" $TEMP_PADDED $FIRMWARE_FINAL

	rm $TEMP_PADDED
}

set -e

if [ ! -d /repo ] || [ ! -d /zephyr ]; then
	echo "Do not execute this file outside of docker container!"
	exit 1
fi

export HOME=/tmp
git config --global --add safe.directory /zephyr/zephyr
source /zephyr/zephyr/zephyr-env.sh

# TODO(b/438110087): Adjust default flow
if [[ -n "$BUILD_MODE" ]]; then
	echo "Generating overlays..."
	cd /repo
	python3 $IMGTOOL gen-overlay
fi

if [[ "$BUILD_MODE" == "COMBINED" ]]; then
	echo "Building combined image..."
	build_bootloader
	build_firmware
        # To combine the images, we discard the last line of the first image
        # which contains the "q" that ends the sections, and just append
        # the second image
	head -n -1 $BOOTLOADER_FINAL > $COMBINED_FINAL && cat $FIRMWARE_FINAL >> $COMBINED_FINAL
        echo "Written combined image at $COMBINED_FINAL"

elif [[ "$BUILD_MODE" == "FIRMWARE" ]]; then
	echo "Building just the firmware..."
	build_firmware
elif [[ "$BUILD_MODE" == "BOOTLOADER" ]]; then
	echo "Building just the bootloader..."
	build_bootloader
else
	echo "Building legacy image..."
	build_legacy
fi
