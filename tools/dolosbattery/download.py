# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a mechanism to download the latest files from the storage bucket."""

import xml.etree.ElementTree as ET
import re
import requests
import yaml

BUCKET_URL = "https://storage.googleapis.com/dolos-firmware"
CABLE_DIR = "cable_eeprom/"


class DownloadException(Exception):
    """General exception for file download errors."""


def get_response(url):
    """Handles the network request and loads it into memory.

    Args:
        url (string): Request URL
    Returns:
        bytes: Response payload
    Raises:
        DownloadException: Response returned an error
    """
    req = requests.get(
        url=url,
        stream=True,
        timeout=180,
    )
    if req.status_code != 200:
        raise DownloadException(f"Request response: {req.status_code}")
    parts = []
    for chunk in req.iter_content(chunk_size=1024):
        parts.append(chunk)
    return b"".join(parts)


def load_config_file(path):
    """Load the config text file."""
    url = BUCKET_URL + "/" + path
    return get_response(url).decode()


def load_cable_list(model=None):
    """Generate the list of cable configs

    Args:
        model (str): Optional filter for the model
    Returns:
        List strings: List of file paths for cable configs
    """

    # The bucket URL response is an XML entry with <Key> tags mapping to
    # mapping the files stored inside.
    list_response = get_response(BUCKET_URL).decode()
    tree = ET.fromstring(list_response)

    # Find all of the <Key> tags and extract the path from each one.
    file_list = [x.text for x in tree.findall("*{*}Key")]

    # Filter the list response to include only the cable directory
    prefix = CABLE_DIR
    if model:
        prefix += model

    # Ignoring case
    prefix = prefix.lower()
    cables = [x for x in file_list if x.lower().startswith(prefix)]

    return cables


def find_latest_config(configs):
    """Return the latest version of the config.

    Config files have a name format matching {MODEL}_v{VERSION}.yaml
    Find the highest model and return it.

    Args:
        configs: List strings with config paths
    Returns:
        string: Path name of the highest version of the yaml config matching
            the model.
    """

    def natsort(path):
        """Simple natural sort function used to find the version name"""
        match = re.search(r".*_v?(\d+)\.yaml$", path, re.IGNORECASE)
        if match:
            return int(match.group(1))
        return -1

    return sorted(configs, key=natsort, reverse=True)[0]


def load_latest_config(model):
    """Loads the yaml describing the battery cable configuration.

    Args:
        model (string): Model name to load
    Returns:
        dict: Cable configuration
    Raises:
        DownloadException: Error reading or parsing the yaml file
    """
    config_list = load_cable_list(model)
    latest_path = find_latest_config(config_list)
    txt = load_config_file(latest_path)
    try:
        table = yaml.safe_load(txt)
        return table
    except yaml.YAMLError as err:
        raise DownloadException(f"Unable to parse yaml file {model}") from err
