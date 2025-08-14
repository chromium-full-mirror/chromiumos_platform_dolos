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


OVERLAY_TEMPLATE_PATH = "tools/imgtool/configs/overlay.template"
BOOTLOADER_OVERLAY_PATH = "bootloader-zephyr/app.overlay"
FIRMWARE_OVERLAY_PATH = "firmware-zephyr/app.overlay"


def setup_parser() -> argparse.ArgumentParser:
    """
    Sets up the argument parser for the image management tool.

    This function defines the command-line interface and its subcommands:
    'pad', 'gen-overlay', and 'get-fw-start-addr'.

    The 'pad' subcommand pads an image file. It allows users to specify an
    image type to pad (legacy, boot, or firmware) using mutually
    exclusive flags, with 'legacy' as the default. It also requires
    input and output file paths.

    The 'gen-overlay' subcommand generates the app.overlay files.

    The 'get-fw-start-addr' subcommand prints the start address of the
    firmware partition in hexadecimal format.

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

    subparsers.add_parser("gen-overlay", help="generate the app.overlay files")
    subparsers.add_parser(
        "get-fw-start-addr",
        help="prints the start address of the firmware partition in hex",
    )
    return parser


def handle_pad_command(image_type: str, input_filename: str, output_filename: str):
    """
    Handles the 'pad' command to resize a binary image to a specified final size.

    This function reads an input binary file, calculates the necessary padding
    to reach a predetermined 'final_size' based on the 'image_type', and
    writes the padded content to an output file.

    Args:
        image_type (str): The type of image to pad (e.g., "legacy").
                          Determines the target `final_size`.
        input_filename (str): The path to the input binary file.
        output_filename (str): The path where the padded output file will be written.

    Raises:
        ValueError: If the input file's size is greater than the calculated
                    `final_size` for the specified `image_type`.
        IOError: If any file input/output operation fails.
    """
    try:
        absolute_input_path = os.path.abspath(input_filename)
        absolute_output_path = os.path.abspath(output_filename)

        if image_type == "legacy":
            final_size = mem_map.FLASH_TOTAL_SIZE
        elif image_type == "boot":
            final_size = mem_map.BOOT_PARTITION_SIZE
        elif image_type == "firmware":
            final_size = mem_map.FIRMWARE_PARTITION_SIZE
        else:
            raise ValueError("Invalid image type")

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


def generate_overlay_file(
    skeleton: str, replacements: dict, code_partition: str, dest_path: str
):
    """Generates an overlay file from a skeleton template and a dictionary of values.

    This function takes a template string, replaces placeholders in the format
    `{SYMBOL}` with corresponding values from the replacements dictionary, and
    writes the result to a specified destination file.

    Args:
        skeleton (str): The template string for the overlay file.
        replacements (dict): A dictionary mapping placeholder symbols to their
            replacement values.
        code_partition (str): The name of the code partition (e.g.,
            'boot_partition') to be inserted for the `{CODE_PARTITION}` symbol.
        dest_path (str): The path to write the generated overlay file to.
    """
    replacements["CODE_PARTITION"] = code_partition
    overlay_contents = skeleton
    for symbol, value in replacements.items():
        overlay_contents = overlay_contents.replace(f"{{{symbol}}}", str(value))
    try:
        with open(dest_path, "w", encoding="utf8") as outfile:
            outfile.write(overlay_contents)
    except IOError as e:
        print(f"File I/O error: {e}")


def handle_gen_overlay_command():
    """Handles the 'gen-overlay' command to create app.overlay files.

    This function reads a skeleton overlay file, populates it with memory map
    constants (e.g., partition sizes and start addresses), and then
    generates two separate `app.overlay` files: one for the bootloader and
    one for the main firmware.
    """

    try:
        with open(OVERLAY_TEMPLATE_PATH, "r", encoding="utf8") as infile:
            raw_overlay_skeleton = infile.read()

            # Discard the copyright lines and any other comments
            filtered_lines = [
                line
                for line in raw_overlay_skeleton.splitlines()
                if not line.startswith("//")
            ]
            overlay_skeleton = "\n".join(filtered_lines)

        replacements = {
            "BOOT_PARTITION_SIZE": hex(mem_map.BOOT_PARTITION_SIZE),
            "FIRMWARE_PARTITION_START_ADDR": hex(mem_map.FIRMWARE_PARTITION_START_ADDR),
            "FIRMWARE_PARTITION_START_ADDR_NO_PREFIX": hex(
                mem_map.FIRMWARE_PARTITION_START_ADDR
            )[2:],
            "FIRMWARE_PARTITION_SIZE": hex(mem_map.FIRMWARE_PARTITION_SIZE),
        }

        generate_overlay_file(
            overlay_skeleton, replacements, "boot_partition", BOOTLOADER_OVERLAY_PATH
        )
        generate_overlay_file(
            overlay_skeleton, replacements, "firmware_partition", FIRMWARE_OVERLAY_PATH
        )

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
    elif args.command == "gen-overlay":
        handle_gen_overlay_command()
    elif args.command == "get-fw-start-addr":
        print(hex(mem_map.FIRMWARE_PARTITION_START_ADDR)[2:])


if __name__ == "__main__":
    main()
