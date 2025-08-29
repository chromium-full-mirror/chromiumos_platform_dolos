# Dolos Image Management Tool

## Overview

This command-line utility manages binary images and configuration files for the **Dolos** project. Its primary functions are:

* **Padding** firmware, bootloader, or legacy binary images to the specific sizes required by the hardware.
* **Generating** the `app.overlay` files for both the bootloader and firmware using memory map constants.
* **Retrieving** the value of a symbol from the memory map.
* **Configuring and appending** the Read-Only partition to the bootloader binary, embedding a version string and the developer flag.
* **Computing and prepending** the CRC32 checksum of the firmware image to the binary.

The tool is integrated into the project's build flow but can also be run manually. It uses constants defined in `configs/memory_map.py` to ensure consistency across the project.

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

### `get` Command

This command prints the value of a symbol from the memory map .This command was added with the intention of making this tool the single source of truth in regards to the memory layout.

**Syntax**
```shell
python3 imgtool.py get [options] symbol_name
```

**Arguments**
* `symbol_name`: The name of the symbol to be printed
* `-x, --hex`(Optional): Flag to print value as hex.

### `append-ro` Command

The `append-ro` command constructs and appends a Read-Only partition to the end of a padded bootloader binary. The partition contains metadata such as the version string for the bootloader and a developer mode flag, to enable skipping of the bootloader timeout.

**Syntax**

```shell
python3 imgtool.py append-ro [options] bootloader_bin version_string
```
**Arguments**
* `bootloader_bin`: The path to the padded bootloader binary.
* `version_string`: The version string to embed in the RO partition. It will be truncated if it exceeds 32 bytes.
* `-d, --dev`: (Optional) Optional flag to indicate a developer build. This sets a flag in the RO partition, and the bootloader will skip the timeout when booting.

### `crc-fw` Command

The `crc-fw` command computes the CRC32 checksum of the padded firmware binary, and appends it to the binary.

**Syntax**

```shell
python3 imgtool.py crc-fw firmware_bin
```

**Arguments**
* `firmware_bin`: The path to the padded firmware binary.
