# Dolos Image Management Tool

## Overview

This command-line utility manages binary images and configuration files for the **Dolos** project. Its primary functions are:

* **Padding** firmware, bootloader, or legacy binary images to the specific sizes required by the hardware.
* **Generating** the `app.overlay` files for both the bootloader and firmware using memory map constants.
* **Retrieving** the firmware partition's starting address for build and scripting purposes.

The tool is integrated into the project's build flow but can also be run manually. It uses constants defined in `configs/memory_map.py` to ensure consistency across the project.

In the future, a `fw-hash` command to prepend the image hash and a `gen-ro` command to generate and append a Read-Only partition will be added.

---

## Usage

The tool is invoked from the command line and uses subcommands to perform different actions.

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

### `gen-overlay` Command

The `gen-overlay` command generates the `app.overlay` files required for building the bootloader and the main firmware. It reads the `configs/overlay.template`, and populates it with the memory layout values stored in `configs/memory_map.py`, creating two distinct files. Symbols are surrounded by `{}` and matched by name: `{SYMBOL}`.

**Syntax**
```shell
python3 imgtool.py gen-overlay
```

This command takes no arguments and will generate

1. `bootloader-zephyr/app.overlay`
2. `firmware-zephyr/app.overlay`

### `get-fw-start-addr` Command

This command prints the start address of the firmware partition. The output is a hexadecimal string without the leading `0x`, suitable for use in other scripts. This command was added with the intention of making this tool the single source of truth in regards to the memory layout.
