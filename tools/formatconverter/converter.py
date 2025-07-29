# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Converts binary image files to the TI-TXT format.

This script provides a command-line utility and a reusable function to transform
raw binary data into the TI-TXT format, used for
programming the Dolos.
"""

import argparse
import io
import os


def convert_binary_to_txt(section_start: int, binary_data: bytes) -> str:
    """
    Reads binary data, converts its content to TI-TXT format
    and returns it as bytestream.

    Args:
        section_start (int): The start address of the section (e.g. 0x0000, 0x8000 etc.)
        binary_data (bytes): The binary data to convert

    Returns:
        A string representing the contents of the .txt file
    """
    outfile = io.StringIO()
    with io.BytesIO(binary_data) as infile:
        outfile.write(f"@{section_start:04x}\n")

        byte_count = 0
        while True:
            # Read 16 bytes at a time to conform to TI-TXT format line length
            chunk = infile.read(16)
            if not chunk:
                break  # End of data

            # Convert chunk to spaced, uppercase hexadecimal string
            # as specified in the TI-TXT format
            # e.g. 18 46 20 20 BD 87 00 00 75 0F 01 00 A9 87 00 00
            hex_string = chunk.hex().upper()
            formatted_hex_line = " ".join(
                hex_string[i : i + 2] for i in range(0, len(hex_string), 2)
            )

            outfile.write(formatted_hex_line + "\n")
            byte_count += len(chunk)

        # Write the final 'q' to terminate the TI-TXT file
        outfile.write("q\n")

    outfile.seek(0)
    return outfile.read()


def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description="Convert a binary image to TI-TXT format"
    )
    parser.add_argument("input_file")
    parser.add_argument("output_file")
    parser.add_argument(
        "-s",
        "--section_start",
        type=lambda x: int(x, base=16),
        help="The starting address of the binary, provided in hexadecimal format",
        default="0x0000",
    )
    args = parser.parse_args()

    # Open the input and output files and process them
    try:

        absolute_input_path = os.path.abspath(args.input_file)
        absolute_output_path = os.path.abspath(args.output_file)

        with open(absolute_input_path, "rb") as infile, open(
            absolute_output_path, "w", encoding="utf8"
        ) as outfile:

            original_data = infile.read()
            print(f"Opened input file '{absolute_input_path}'")
            output_data = convert_binary_to_txt(args.section_start, original_data)
            outfile.write(output_data)
            print(f"Written result to '{absolute_output_path}'")

    except IOError as e:
        print(f"File I/O error: {e}")


if __name__ == "__main__":
    main()
