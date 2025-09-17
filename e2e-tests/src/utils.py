# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=missing-module-docstring


import pathlib

from google.cloud import storage


FIRMWARE_CLOUD_BUCKET = "dolos-firmware"
BOX_FIRMWARE_SUBDIR = "box_firmware/"
BOOTLOADER_SUBDIR = "bootloader/"


def _get_versions_helper(for_bootloader: bool):
    """Retrieves available Dolos firmware or bootloader versions.

    Fetches a list of available versions from a Google Cloud Storage
    bucket.
    Returns:
        A list containing all available versions, sorted in
        descending order
    """
    result = []
    storage_client = storage.Client.create_anonymous_client()
    prefix = BOX_FIRMWARE_SUBDIR
    version_index = 1
    path_parts_count = 3
    if for_bootloader:
        prefix += BOOTLOADER_SUBDIR
        version_index = 2
        path_parts_count = 4

    for blob in storage_client.list_blobs(FIRMWARE_CLOUD_BUCKET, prefix=prefix):
        path = pathlib.PurePath(blob.name)
        if len(path.parts) != path_parts_count:
            continue
        if not for_bootloader and BOOTLOADER_SUBDIR in str(path):
            continue
        version = path.parts[version_index]
        result.append(version)
    result.sort(reverse=True)
    return result


def get_firmware_versions():
    """Retrieves available Dolos firmware versions.

    Fetches a list of available firmware versions from a Google Cloud Storage
    bucket.

    Returns:
        A list containing all firmware versions, in descending order.
    """
    return list(
        filter(
            lambda version: version.startswith("1."),
            _get_versions_helper(for_bootloader=False),
        )
    )


def get_bootloader_versions():
    """Retrieves available Dolos bootloader versions.

    Fetches a list of available bootloader versions from a Google Cloud Storage
    bucket.

    Returns:
        A list containing all bootloader versions, in descending order.
    """
    return list(
        filter(
            lambda version: version.startswith("2."),
            _get_versions_helper(for_bootloader=True),
        )
    )
