# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the imgtool.py module
"""

# pylint: disable=import-error


import argparse
import os

import configs.memory_map as mem_map
import imgtool.imgtool as imgtool_script
import pytest


# Define some constants for test file names and paths
TEST_DIR = "test_artifacts"


@pytest.fixture(autouse=True)
def setup_and_teardown_test_dir():
    """Fixture to create and clean up a test directory for artifacts."""
    if not os.path.exists(TEST_DIR):
        os.makedirs(TEST_DIR)
    yield
    # Clean up after each test
    for f in os.listdir(TEST_DIR):
        os.remove(os.path.join(TEST_DIR, f))
    os.rmdir(TEST_DIR)


@pytest.fixture
def create_temp_file():
    """Fixture to create a temporary input file with specified content."""

    def _create_temp_file(filename: str, content: bytes):
        filepath = os.path.join(TEST_DIR, filename)
        with open(filepath, "wb") as f:
            f.write(content)
        return filepath

    return _create_temp_file


# --- Tests for setup_parser ---


def test_setup_parser_returns_argparser_instance():
    """Verify setup_parser returns an ArgumentParser object."""
    parser = imgtool_script.setup_parser()
    assert isinstance(parser, argparse.ArgumentParser)


def test_setup_parser_has_pad_subcommand():
    """Verify 'pad' subcommand is registered."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(["pad", "input.bin", "output.bin"])
    assert args.command == "pad"


def test_setup_parser_pad_subcommand_has_input_output_files():
    """Verify 'pad' subcommand requires input and output files."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(["pad", "my_input.bin", "my_output.bin"])
    assert args.input_file == "my_input.bin"
    assert args.output_file == "my_output.bin"


def test_setup_parser_pad_subcommand_default_image_type_is_legacy():
    """Verify default image_type for 'pad' is 'legacy'."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(["pad", "input.bin", "output.bin"])
    assert args.image_type == "legacy"


@pytest.mark.parametrize(
    "flag, expected_type",
    [
        ("-l", "legacy"),
        ("--legacy", "legacy"),
        ("-b", "boot"),
        ("--boot", "boot"),
        ("-f", "firmware"),
        ("--firmware", "firmware"),
    ],
)
def test_setup_parser_pad_subcommand_image_type_flags(flag, expected_type):
    """Verify image type flags correctly set the image_type."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(["pad", flag, "input.bin", "output.bin"])
    assert args.image_type == expected_type


def test_setup_parser_pad_subcommand_mutually_exclusive_flags():
    """Verify image type flags are mutually exclusive."""
    parser = imgtool_script.setup_parser()
    with pytest.raises(SystemExit):  # argparse raises SystemExit on error
        parser.parse_args(["pad", "-l", "-b", "input.bin", "output.bin"])


# --- Tests for handle_pad_command ---


def test_handle_pad_command_pads_legacy_image_correctly(create_temp_file):
    """Test padding a legacy image when input is smaller than final size."""
    input_content = b"\x01\x02\x03\x04"
    input_file = create_temp_file("input_legacy.bin", input_content)
    output_file = os.path.join(TEST_DIR, "output_legacy_padded.bin")

    imgtool_script.handle_pad_command("legacy", input_file, output_file)

    expected_pad_bytes = (
        mem_map.FLASH_TOTAL_SIZE - len(input_content)
    ) * mem_map.PAD_VALUE.to_bytes(1, "little")
    expected_output_content = input_content + expected_pad_bytes

    assert os.path.exists(output_file)
    with open(output_file, "rb") as f:
        actual_output_content = f.read()

    assert actual_output_content == expected_output_content
    assert len(actual_output_content) == mem_map.FLASH_TOTAL_SIZE


def test_handle_pad_command_raises_value_error_if_input_too_large(create_temp_file):
    """Test that ValueError is raised if input file is larger than final size."""
    # Create an input file larger than FLASH_TOTAL_SIZE
    input_content = b"\x00" * (mem_map.FLASH_TOTAL_SIZE + 1)
    input_file = create_temp_file("input_too_large.bin", input_content)
    output_file = os.path.join(TEST_DIR, "output_error.bin")

    with pytest.raises(ValueError, match="Input size: .* larger than final size: .*!"):
        imgtool_script.handle_pad_command("legacy", input_file, output_file)
