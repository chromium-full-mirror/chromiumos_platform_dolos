# Dolos Image Management Tool

## Overview

This command-line utility manages binary images for the **Dolos** project. Its primary function is to **pad** firmware, bootloader, or legacy binary images to the specific size required.

In the future, a fw-sign command to prepend the image hash to the firmware image, a gen-overlay command to generate the correct app.overlay files, and a gen-ro to generate and append a Read Only partition to the bootloader image will be added.

The tool is integrated into the project's build flow but can also be run manually to handle image resizing. It works by taking an input binary, calculating the necessary padding based on the image type, and appending bytes (`0xFF`) until the target size is reached.

The flash memory layout and associated constants the tool uses are defined in `configs/memory_map.py`.

---

## Usage

The tool is invoked from the command line and uses subcommands to perform different actions. Currently, only the `pad` command is implemented.

### `pad` Command

The `pad` command resizes a binary image to a final size determined by its type.

**Syntax:**
```shell
python3 imgtool.py pad [options] input_file output_file
```
**Arguments**

- `input_file`: The path to the source binary file
- `output_file`: The path to where the padded output will be written

**Example**

To pad a binary file named zephyr.bin for a legacy build, you can run:
```
python3 imgtool.py pad --legacy zephyr.bin zephyr.padded.bin
```
