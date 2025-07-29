# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for converter.py"""

import sys

from formatconverter.converter import convert_binary_to_txt
from formatconverter.converter import main


# --- Test cases for convert_binary_to_txt function ---


def test_empty_binary_data():
    """Test empty input"""
    section_start = 0x1000
    binary_data = b""
    expected_output = "@1000\nq\n"
    assert convert_binary_to_txt(section_start, binary_data) == expected_output


def test_single_byte_data():
    """Test single byte input"""
    section_start = 0x0
    binary_data = b"\xab"
    expected_output = "@0000\nAB\nq\n"
    assert convert_binary_to_txt(section_start, binary_data) == expected_output


def test_less_than_16_bytes_data():
    """Test input with less than 16 bytes"""
    section_start = 0x100
    binary_data = b"\x01\x02\x03\x04\x05"
    expected_output = "@0100\n01 02 03 04 05\nq\n"
    assert convert_binary_to_txt(section_start, binary_data) == expected_output


def test_exact_16_bytes_data():
    """Test input with size exactly 16 bytes"""
    section_start = 0x200
    binary_data = b"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff"
    expected_output = "@0200\n00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\nq\n"
    assert convert_binary_to_txt(section_start, binary_data) == expected_output


def test_multiple_full_chunks_and_partial_end():
    """Test input with multiple 16 byte chunks and a partial chunk at the end"""
    section_start = 0x300
    # 16 bytes + 5 bytes
    binary_data = (
        b"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff"
        b"\x0a\x0b\x0c\x0d\x0e"
    )
    expected_output = (
        "@0300\n"
        "00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\n"
        "0A 0B 0C 0D 0E\n"
        "q\n"
    )
    assert convert_binary_to_txt(section_start, binary_data) == expected_output


def test_section_start_zero():
    """Test writing a section at start address 0x0"""
    section_start = 0x0
    binary_data = b"\x01"
    expected_output = "@0000\n01\nq\n"
    assert convert_binary_to_txt(section_start, binary_data) == expected_output


def test_section_start_not_zero():
    """Test writing a section with a non zero start address"""
    section_start = 0xFF
    binary_data = b"\x12"
    expected_output = "@00ff\n12\nq\n"
    assert convert_binary_to_txt(section_start, binary_data) == expected_output


# --- End-to-end test for file conversion ---


def test_e2e_file_conversion(tmp_path, monkeypatch):
    """
    Tests the full script execution flow by simulating command-line arguments
    and verifying the output file content.
    Uses pytest's tmp_path for temporary files and monkeypatch for sys.argv.
    """
    input_filename = tmp_path / "test_input.bin"
    output_filename = tmp_path / "test_output.txt"

    test_binary_data = b"\xde\xad\xbe\xef" * 4  # 16 bytes
    expected_file_content = (
        "@0000\nDE AD BE EF DE AD BE EF DE AD BE EF DE AD BE EF\nq\n"
    )

    # Write test input file
    input_filename.write_bytes(test_binary_data)

    # Simulate command-line arguments using monkeypatch
    monkeypatch.setattr(
        sys,
        "argv",
        ["converter.py", str(input_filename), str(output_filename), "-s", "0x0000"],
    )

    main()

    actual_output_content = output_filename.read_text()
    assert actual_output_content == expected_file_content
