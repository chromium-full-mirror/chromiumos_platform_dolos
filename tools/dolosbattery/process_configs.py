#!/usr/bin/python3
# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=too-few-public-methods

"""Processes the Battery Configs."""

import argparse
import logging
from multiprocessing import Pool
import pathlib

from dolosbattery import logs
from dolosbattery.configs import Config
from dolosbattery.hwid_repo import HWIDModel
from doloscmd import download


def _download_config(args):
    """Download a single model's latest configuration file.

    This function is designed to be used with multiprocessing to download
    configuration files for multiple models concurrently.

    Args:
        args (tuple): A tuple containing the model name (str) and the
                      directory to save the configuration file (pathlib.Path).
    """

    model, save_dir = args
    logging.info("Downloading %s", model)
    config_list = download.load_cable_list(model)
    latest_path = download.find_latest_config(config_list)
    name = latest_path.split("/")[-1]
    cfg_path = save_dir / name
    txt = download.load_config_file(latest_path)
    cfg_path.write_text(txt)


def download_all_configs(save_dir):
    """Download the latest configuration files for all models.

    This function retrieves the list of available models, and then downloads
    the latest configuration file for each model, saving them to the specified
    directory.

    Args:
        save_dir (pathlib.Path): The directory where the downloaded
                                 configuration files will be saved.
    """

    if not save_dir.exists():
        save_dir.mkdir()

    cables = download.load_cable_list()
    models = [x.split("/")[1] for x in cables]

    args = [(x, save_dir) for x in models]
    with Pool() as p:
        p.map(_download_config, args)


def main():
    """Parse command-line arguments and process battery configurations.

    This function serves as the entry point for the script. It parses
    command-line arguments, sets up logging, optionally downloads
    configuration files, builds the HWID map, and processes the
    configuration files.
    """

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-id",
        dest="hwid_repo",
        type=pathlib.Path,
        required=True,
        help="File path to the root of the HWID repo",
    )
    parser.add_argument(
        "-i",
        dest="input",
        type=pathlib.Path,
        required=True,
        help="File path use for input configs",
    )
    parser.add_argument(
        "-o",
        dest="output",
        type=pathlib.Path,
        required=True,
        help="File path use for output configs",
    )
    parser.add_argument(
        "-v", dest="verbose", action="store_true", help="Enable verbose logging"
    )
    parser.add_argument(
        "-d",
        dest="download",
        action="store_true",
        help="Download latest configs to input",
    )
    args = parser.parse_args()

    logs.set_config(args.verbose)

    if args.download:
        download_all_configs(args.input)

    HWIDModel.build_hwid_map(args.hwid_repo)

    Config.parse_cfg_files(args.input, args.output)


if __name__ == "__main__":
    main()
