# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the imgtool.py module
"""

# pylint: disable=import-error
# pylint: disable=protected-access, redefined-outer-name

import argparse
import os
import zlib

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


# --- Tests for setup_parser (append-ro) ---


def test_setup_parser_has_append_ro_subcommand():
    """Verify 'append-ro' subcommand is registered."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(["append-ro", "boot.bin", "v1.0"])
    assert args.command == "append-ro"


def test_setup_parser_append_ro_subcommand_arguments():
    """Verify 'append-ro' subcommand requires bootloader and version args."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(["append-ro", "my_boot.bin", "v1.2.3-test"])
    assert args.bootloader_bin == "my_boot.bin"
    assert args.version_string == "v1.2.3-test"


@pytest.mark.parametrize(
    "args_list, expected_dev_flag",
    [
        (["append-ro", "b.bin", "v1"], False),
        (["append-ro", "b.bin", "v1", "-d"], True),
        (["append-ro", "b.bin", "v1", "--dev"], True),
    ],
)
def test_setup_parser_append_ro_dev_flag(args_list, expected_dev_flag):
    """Verify the '-d/--dev' flag sets the dev attribute."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(args_list)
    assert args.dev is expected_dev_flag


# --- Tests for setup_parser (crc-fw) ---


def test_setup_parser_has_crc_fw_subcommand():
    """Verify 'crc-fw' subcommand is registered."""
    parser = imgtool_script.setup_parser()
    args = parser.parse_args(["crc-fw", "firmware.bin"])
    assert args.command == "crc-fw"
    assert args.firmware_bin == "firmware.bin"


def test_setup_parser_crc_fw_requires_argument():
    """Verify 'crc-fw' subcommand requires the firmware_bin argument."""
    parser = imgtool_script.setup_parser()
    with pytest.raises(SystemExit):
        parser.parse_args(["crc-fw"])


# --- Tests for handle_pad_command ---


def test_handle_pad_command_pads_legacy_image_correctly(create_temp_file):
    """Test padding a legacy image when input is smaller than final size."""
    input_content = b"\x01\x02\x03\x04"
    input_file = create_temp_file("input_legacy.bin", input_content)
    output_file = os.path.join(TEST_DIR, "output_legacy_padded.bin")

    imgtool_script.handle_pad_command("legacy", input_file, output_file)

    expected_pad_bytes = (
        mem_map.FLASH_TOTAL_SIZE - len(input_content)
    ) * mem_map.PAD_VALUE
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


# --- Tests for internal helper functions ---


def test_create_version_bytes_standard_string():
    """Verify version string is correctly encoded, null-terminated, and padded."""
    version_str = "v1.0.0"
    expected_bytes = (version_str.encode("utf-8") + b"\x00").ljust(
        mem_map.RO_RECORD_SIZE, mem_map.PAD_VALUE
    )
    result = imgtool_script._create_version_record(version_str)
    assert result == expected_bytes
    assert len(result) == mem_map.RO_RECORD_SIZE


def test_create_version_bytes_long_string_is_truncated(capsys):
    """Verify a long version string is truncated and a warning is printed."""
    long_str = "a" * (mem_map.RO_RECORD_SIZE + 10)
    expected_bytes = (long_str.encode("utf-8"))[: mem_map.RO_RECORD_SIZE - 1] + b"\x00"

    result = imgtool_script._create_version_record(long_str)
    assert result == expected_bytes
    assert len(result) == mem_map.RO_RECORD_SIZE

    captured = capsys.readouterr()
    assert "Warning: Version string >" in captured.out
    assert "will be truncated" in captured.out


def test_create_version_bytes_empty_string():
    """Verify an empty version string results in a null terminator and padding."""
    expected_bytes = b"\x00".ljust(mem_map.RO_RECORD_SIZE, mem_map.PAD_VALUE)
    result = imgtool_script._create_version_record("")
    assert result == expected_bytes


def test_create_flags_record_dev_true():
    """Verify dev=True creates a record with the dev bit cleared (0xFE)."""
    # Negative logic: dev=True means bit is 0. 0xFF -> 11111111, 0xFE -> 11111110
    expected_bytes = b"\xfe".ljust(mem_map.RO_RECORD_SIZE, mem_map.PAD_VALUE)
    result = imgtool_script._create_flags_record(dev_flag=True)
    assert result == expected_bytes


def test_create_flags_record_dev_false():
    """Verify dev=False creates a record with the dev bit set (0xFF)."""
    # Negative logic: dev=False means bit is 1. 0xFF -> 11111111
    expected_bytes = b"\xff" * mem_map.RO_RECORD_SIZE
    result = imgtool_script._create_flags_record(dev_flag=False)
    assert result == expected_bytes


def test_build_ro_partition_data_structure_and_size():
    """Verify the full RO partition data is constructed correctly."""
    version_str = "test-version"
    dev_flag = True

    # Get expected components
    version_bytes = imgtool_script._create_version_record(version_str)
    flags_bytes = imgtool_script._create_flags_record(dev_flag)
    metadata = version_bytes + flags_bytes
    padding = mem_map.PAD_VALUE * (mem_map.RO_PARTITION_SIZE - len(metadata))
    expected_data = metadata + padding

    # Build the actual data
    actual_data = imgtool_script._build_ro_partition_data(version_str, dev_flag)

    assert len(actual_data) == mem_map.RO_PARTITION_SIZE
    assert actual_data == expected_data


# --- Tests for handle_append_ro_command ---


def test_handle_append_ro_command_appends_correctly(create_temp_file):
    """Test that the RO partition is correctly appended to the bootloader binary."""
    bootloader_content = b"\xde\xad\xbe\xef" * 10
    bootloader_file = create_temp_file("bootloader.bin", bootloader_content)
    version_str = "final-v2"
    dev_flag = False

    # Get the expected RO data that should be appended
    expected_ro_data = imgtool_script._build_ro_partition_data(version_str, dev_flag)

    # Run the command
    imgtool_script.handle_append_ro_command(bootloader_file, version_str, dev_flag)

    # Verify the result
    expected_final_content = bootloader_content + expected_ro_data
    with open(bootloader_file, "rb") as f:
        actual_final_content = f.read()

    assert (
        len(actual_final_content) == len(bootloader_content) + mem_map.RO_PARTITION_SIZE
    )
    assert actual_final_content == expected_final_content


# --- Tests for handle_crc_fw_command ---


def test_handle_crc_fw_command_prepends_crc_correctly(create_temp_file):
    """Test that the CRC32 trailer is correctly prepended to the firmware."""
    original_content = b"This is the firmware content for CRC32 calculation."
    firmware_file = create_temp_file("firmware_to_crc.bin", original_content)
    original_size = len(original_content)

    # Calculate the expected trailer based on CRC32
    expected_crc_int = zlib.crc32(original_content) & 0xFFFFFFFF
    expected_crc_bytes = expected_crc_int.to_bytes(4, byteorder="little")

    padding_size = mem_map.FIRMWARE_TRAILER_SIZE - len(expected_crc_bytes)
    padding = mem_map.PAD_VALUE * padding_size
    expected_trailer = expected_crc_bytes + padding
    expected_final_content = original_content + expected_trailer

    # Run the command to modify the file
    imgtool_script.handle_crc_fw_command(firmware_file)

    # Verify the file's new content
    with open(firmware_file, "rb") as f:
        actual_final_content = f.read()

    assert len(actual_final_content) == original_size + mem_map.FIRMWARE_TRAILER_SIZE
    assert actual_final_content == expected_final_content
    assert actual_final_content.startswith(original_content)
    assert actual_final_content.endswith(expected_trailer)


def test_handle_crc_fw_command_with_empty_file(create_temp_file, capsys):
    """Test behavior with an empty input file, which should result in a warning."""
    firmware_file = create_temp_file("empty_firmware.bin", b"")

    imgtool_script.handle_crc_fw_command(firmware_file)

    # Verify the file remains empty
    assert os.path.getsize(firmware_file) == 0

    # Verify the warning message was printed to stdout
    captured = capsys.readouterr()
    assert "Warning: The file" in captured.out
    assert "is empty" in captured.out


def test_handle_crc_fw_command_file_not_found(capsys):
    """Test the error message when the input firmware file does not exist."""
    non_existent_file = os.path.join(TEST_DIR, "non_existent_file.bin")

    imgtool_script.handle_crc_fw_command(non_existent_file)

    captured = capsys.readouterr()
    assert f"Error: The file '{non_existent_file}' could not be found." in captured.out


def test_handle_crc_fw_command_prints_error_if_checksum_is_too_large(
    create_temp_file, monkeypatch, capsys
):
    """
    Test that an error is printed if the checksum is larger than the trailer size.
    """
    firmware_content = b"some data"
    firmware_file = create_temp_file("fw_small_trailer.bin", firmware_content)

    # Mock the trailer size to be smaller than a CRC32 checksum (4 bytes)
    monkeypatch.setattr(mem_map, "FIRMWARE_TRAILER_SIZE", 2)

    # Run the function that is expected to print an error
    imgtool_script.handle_crc_fw_command(firmware_file)

    # Capture the output from stdout and stderr
    captured = capsys.readouterr()

    # Assert that the expected error message is in the captured output
    assert "An unexpected error occurred" in captured.out
    assert "checksum length (4 bytes) is greater than" in captured.out
