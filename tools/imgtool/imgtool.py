# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Command line utility to manage the binary images generated for the Dolos project.
"""

# pylint: skip-file
# pylint: disable=W
# pylint: disable=import-error, redefined-outer-name, protected-access

import argparse
import os
import zlib

import configs.memory_map as mem_map


OVERLAY_TEMPLATE_PATH = "tools/imgtool/configs/overlay.template"
BOOTLOADER_OVERLAY_PATH = "bootloader-zephyr/app.overlay"
FIRMWARE_OVERLAY_PATH = "firmware-zephyr/app.overlay"


def setup_parser() -> argparse.ArgumentParser:
    """
    Sets up the argument parser for the image management tool.

    This function defines the command-line interface and its subcommands.
    For complete details see README.md

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
    get_parser = subparsers.add_parser(
        "get",
        help="prints the value of the specified symbol in mem_map",
    )
    get_parser.add_argument("symbol_name")
    get_parser.add_argument("-x", "--hex", action="store_true", help="print as hex")

    append_ro_parser = subparsers.add_parser(
        "append-ro", help="append a configured RO partition to the bootloader binary"
    )
    append_ro_parser.add_argument(
        "bootloader_bin", help="path to padded bootloader binary"
    )
    append_ro_parser.add_argument("version_string")
    append_ro_parser.add_argument(
        "-d",
        "--dev",
        action="store_true",
        help="enable developer flag to skip bootloader timeout",
    )

    crc_fw_parser = subparsers.add_parser(
        "crc-fw", help="append the CRC32 of the firmware to its binary"
    )
    crc_fw_parser.add_argument(
        "firmware_bin", help="path of the padded firmware binary"
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
            output_bytes = input_bytes + pad_size * mem_map.PAD_VALUE

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


def handle_get_command(symbol_name: str, hex_mode: bool):
    """Prints the value of a symbol from the mem_map module."""
    try:
        # Use getattr() to get the value of the symbol by its string name.
        value = getattr(mem_map, symbol_name)
        if hex_mode:
            print(hex(value))
        else:
            print(value)
    except AttributeError:
        # Handle cases where the symbol name is not found.
        print(f"Error: Symbol '{symbol_name}' not found.")


# fmt: off
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
            "RO_PARTITION_START_ADDR": hex(mem_map.RO_PARTITION_START_ADDR),
            "RO_PARTITION_START_ADDR_NO_PREFIX": hex(
                mem_map.RO_PARTITION_START_ADDR
            )[2:],
            "RO_PARTITION_SIZE": hex(mem_map.RO_PARTITION_SIZE),
            "FIRMWARE_TRAILER_START_ADDR" : hex(mem_map.FIRMWARE_TRAILER_START_ADDR),
            "FIRMWARE_TRAILER_START_ADDR_NO_PREFIX" : hex(
                mem_map.FIRMWARE_TRAILER_START_ADDR
            )[2:],
            "FIRMWARE_TRAILER_SIZE" : hex(mem_map.FIRMWARE_TRAILER_SIZE),
        }

        generate_overlay_file(
            overlay_skeleton, replacements, "boot_partition", BOOTLOADER_OVERLAY_PATH
        )
        generate_overlay_file(
            overlay_skeleton, replacements, "firmware_partition", FIRMWARE_OVERLAY_PATH
        )

    except IOError as e:
        print(f"File I/O error: {e}")


def _create_version_record(version_string: str) -> bytes:
    """Encodes, null-terminates, truncates, and pads
    the version string to RO_RECORD_SIZE bytes.
    """
    version_bytes = version_string.encode("utf-8")
    if len(version_bytes) > mem_map.RO_RECORD_SIZE:
        print(
            f"Warning: Version string > {mem_map.RO_RECORD_SIZE}"
            + " bytes, will be truncated."
        )
        version_bytes = version_bytes[:(mem_map.RO_RECORD_SIZE-1)]
    # NULL terminate the string
    version_bytes += b'\x00'
    # Pad the remainder with 0xFF
    return version_bytes.ljust(mem_map.RO_RECORD_SIZE, mem_map.PAD_VALUE)
# fmt: on


def _create_flags_record(dev_flag: bool) -> bytes:
    """
    Creates the flags record using negative logic
    (0 for True, 1 for False).
    """
    # LSB bit is the dev toggle. It's negative logic because
    # the flash erase value is 0xFF.
    # A 'True' dev_flag means we need to set the bit to 0.
    dev_value = b"\xfe" if dev_flag else b"\xff"
    dev_record_bytes = (
        dev_value + (mem_map.RO_RECORD_SIZE - len(dev_value)) * mem_map.PAD_VALUE
    )
    return dev_record_bytes


def _build_ro_partition_data(version_string: str, dev_flag: bool) -> bytes:
    """Constructs the complete RO partition data block."""
    version_data = _create_version_record(version_string)
    flags_data = _create_flags_record(dev_flag)

    metadata = version_data + flags_data

    # Ensure the metadata doesn't already exceed the total partition size.
    if len(metadata) > mem_map.RO_PARTITION_SIZE:
        raise ValueError("RO metadata size exceeds the total RO partition size.")

    # Calculate padding to fill the rest of the partition.
    padding_size = mem_map.RO_PARTITION_SIZE - len(metadata)
    padding = mem_map.PAD_VALUE * padding_size

    return metadata + padding


def handle_append_ro_command(
    bootloader_binary: str, version_string: str, dev_flag: bool
):
    """Appends a configured RO partition to the bootloader binary.

    Args:
        bootloader_binary (str): Path to the padded bootloader binary.
        version_string (str): The version string to embed.
        dev_flag (bool): Flag to indicate a developer build.
    """
    try:
        # Build the entire RO partition data in a helper function.
        ro_partition_data = _build_ro_partition_data(version_string, dev_flag)

        # Append the constructed data block to the binary file.
        with open(bootloader_binary, "ab") as outfile:
            bytes_written = outfile.write(ro_partition_data)
            print(f"Appended {bytes_written}B RO partition to '{bootloader_binary}'")

    except (IOError, ValueError) as e:
        print(f"Error: {e}")


def handle_crc_fw_command(firmware_binary: str):
    """
    Reads the firmware binary, computes its CRC32 checksum, pads the checksum
    to a fixed trailer size, and appends this new trailer to the original file.

    Args:
        firmware_binary (str): The file path to the firmware binary.
    """
    try:
        with open(firmware_binary, "rb") as f:
            firmware_content = f.read()

        if not firmware_content:
            print(f"Warning: The file '{firmware_binary}' is empty.")
            return

        # Calculate CRC32 checksum. zlib.crc32 returns a signed integer,
        # so we use & 0xFFFFFFFF to get an unsigned 32-bit value.
        crc32_checksum_int = zlib.crc32(firmware_content) & 0xFFFFFFFF

        # Convert the integer checksum to a 4-byte little-endian byte string.
        crc32_checksum_bytes = crc32_checksum_int.to_bytes(4, byteorder="little")
        checksum_len = len(crc32_checksum_bytes)

        trailer_size = mem_map.FIRMWARE_TRAILER_SIZE
        if checksum_len > trailer_size:
            raise ValueError(
                f"Error: The checksum length ({checksum_len} bytes) is greater than the "
                f"defined trailer size ({trailer_size} bytes)."
            )

        padding_size = trailer_size - checksum_len
        padding = mem_map.PAD_VALUE * padding_size

        firmware_trailer = crc32_checksum_bytes + padding

        with open(firmware_binary, "ab+") as f:
            f.write(firmware_trailer)

        print(
            f"Successfully added a {trailer_size}-byte CRC32 trailer to '{firmware_binary}'."
        )
        print(f"  - CRC32 Checksum: 0x{crc32_checksum_int:08x}")
        print(f"  - Original file size: {len(firmware_content)} bytes")
        print(f"  - New file size: {os.path.getsize(firmware_binary)} bytes")

    except FileNotFoundError:
        print(f"Error: The file '{firmware_binary}' could not be found.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


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
    elif args.command == "get":
        handle_get_command(args.symbol_name, args.hex)
    elif args.command == "append-ro":
        handle_append_ro_command(args.bootloader_bin, args.version_string, args.dev)
    elif args.command == "crc-fw":
        handle_crc_fw_command(args.firmware_bin)


if __name__ == "__main__":
    main()
