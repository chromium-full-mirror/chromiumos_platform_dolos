# Get Dolos Devices Script

This script queries the fleet database to get a list of all DUTs that have a Dolos device connected. It can be used to generate a full list of devices or a staged list for a phased rollout.

## Prerequisites

- The `f1-sql` command-line tool installed and configured.

## How to Use

**Note:** All commands should be run from the `tools/dolos_fleet_rollout` directory.

Run the script from the command line, specifying the output file and optionally the rollout stage and target firmware version.

### Get all devices that need an update

```bash
./get_dolos_devices.py --output-file all_dolos_devices.csv --fw-version <firmware_version>
```

### Get a staged list of devices that need an update

```bash
./get_dolos_devices.py --output-file staged_dolos_devices.csv --stage first --fw-version <firmware_version>
```

### Arguments

*   `--output-file` (required): The path where the output CSV file will be saved.
*   `--stage` (optional): The rollout stage. Defaults to `all`.
    *   `first`: Selects ~10% of devices, with at least one from each board type.
    *   `second`: Randomly selects 33% of all devices.
    *   `all`: Selects all devices.
*   `--fw-version` (optional): The target firmware version. If provided, the script will only include devices that do not already have this version.

### Output

The script will create a CSV file at the specified path containing the list of selected Dolos devices and their associated information. This file can then be used as input for the `dolos_fleet_rollout.py` script.
