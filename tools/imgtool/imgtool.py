# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Command line utility to manage the binary images generated for the Dolos project.
"""

# pylint: disable=import-error


import argparse
import os

import configs.memory_map as mem_map


def setup_parser() -> argparse.ArgumentParser:
    """
    Sets up the argument parser for the image management tool.

    This function defines the command-line interface, including the 'pad'
    subcommand and its associated arguments. The 'pad' subcommand allows
    users to specify an image type to pad (legacy, boot, or firmware)
    using mutually exclusive flags, with 'legacy' as the default if no
    type is explicitly chosen. It also requires input and output file paths.

    Returns:
        argparse.ArgumentParser: The configured argument parser object.
    """
    parser = argparse.ArgumentParser(description="Image management tool")
    subparsers = parser.add_subparsers(dest="command", help="subcommand help")

    parser_pad = subparsers.add_parser("pad", help="pad image")
    image_type_group = parser_pad.add_mutually_exclusive_group()
    image_type_group.add_argument(
        "-l",
        "--legacy",
        action="store_const",
        const="legacy",
        dest="image_type",
        help="pad a legacy firmware image to fill the entire flash memory",
    )
    image_type_group.add_argument(
        "-b",
        "--boot",
        action="store_const",
        const="boot",
        dest="image_type",
        help="pad a bootloader image",
    )
    image_type_group.add_argument(
        "-f",
        "--firmware",
        action="store_const",
        const="firmware",
        dest="image_type",
        help="pad a firmware image",
    )

    # Set the default value for image_type if none of the flags are given
    # TODO(b/438110087): Adjust flags when converting the default flow
    parser_pad.set_defaults(image_type="legacy")

    parser_pad.add_argument("input_file")
    parser_pad.add_argument("output_file")

    return parser


def handle_pad_command(image_type: str, input_filename: str, output_filename: str):
    """
    Handles the 'pad' command to resize a binary image to a specified final size.

    This function reads an input binary file, calculates the necessary padding
    to reach a predetermined 'final_size' based on the 'image_type', and
    writes the padded content to an output file. Currently, only 'legacy'
    image type padding is implemented.

    Args:
        image_type (str): The type of image to pad (e.g., "legacy").
                          Determines the target `final_size`.
        input_filename (str): The path to the input binary file.
        output_filename (str): The path where the padded output file will be written.

    Raises:
        NotImplementedError: If the `image_type` is not "legacy" (as other types
                             are not yet implemented).
        ValueError: If the input file's size is greater than the calculated
                    `final_size` for the specified `image_type`.
        IOError: If any file input/output operation fails.
    """
    try:
        absolute_input_path = os.path.abspath(input_filename)
        absolute_output_path = os.path.abspath(output_filename)

        if image_type == "legacy":
            final_size = mem_map.FLASH_TOTAL_SIZE
        else:
            raise NotImplementedError("Pad type not implemented")

        with open(absolute_input_path, "rb") as infile, open(
            absolute_output_path, "wb"
        ) as outfile:
            input_bytes = infile.read()
            input_size = len(input_bytes)
            print(f"Read {input_size}B from input file '{absolute_input_path}'")

            if input_size > final_size:
                raise ValueError(
                    f"Input size: {input_size} larger than final size: {final_size}!"
                )

            pad_size = final_size - input_size
            output_bytes = input_bytes + pad_size * mem_map.PAD_VALUE.to_bytes(
                1, "little"
            )

            outfile.write(output_bytes)
            print(f"Written {final_size}B to output file '{absolute_output_path}'")

    except IOError as e:
        print(f"File I/O error: {e}")


def main():
    """Main driver function"""

    parser = setup_parser()
    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        return

    if args.command == "pad":
        handle_pad_command(args.image_type, args.input_file, args.output_file)


if __name__ == "__main__":
    main()
