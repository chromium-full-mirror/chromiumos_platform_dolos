# Dolos Firmware Release Playbook

This document outlines the process for rolling out a new Dolos firmware version to the fleet.

## Prerequisites

To use the scripts in this playbook, you will need:

- gcert
- SSH access to labstations in the fleet.
- The `shivas` command-line tool installed and logged in.
- The `f1-sql` command-line tool installed and configured.
- The `doloscmd` tool must be available on the labstations being accessed.

**Note:** All commands in this playbook should be run from the `tools/dolos_fleet_rollout` directory.

## 1. Generate Staged Device Lists

The first step is to generate CSV files for each stage of the rollout using the `get_dolos_devices.py` script. Provide the target firmware version to automatically exclude devices that are already up to date.

### Stage 1: Initial Rollout (~10%)

```bash
./get_dolos_devices.py --stage first --output-file dolos_stage1.csv --fw-version <firmware_version>
```

### Stage 2: Second Rollout (33%)

```bash
./get_dolos_devices.py --stage second --output-file dolos_stage2.csv --fw-version <firmware_version>
```

### All Devices

```bash
./get_dolos_devices.py --stage all --output-file all_dolos_devices.csv --fw-version <firmware_version>
```

## 2. Start the Rollout

Once you have the device lists, you can start the rollout using the `dolos_fleet_rollout.py` script, starting with stage 1.

```bash
./dolos_fleet_rollout.py --csv-file dolos_stage1.csv --set-version <firmware_version> --repair
```

## 3. Monitor the Rollout

After starting the rollout for a stage, you need to monitor its progress using the `get_dolos_versions.py` script.

```bash
./get_dolos_versions.py --input-csv dolos_stage1.csv --output-csv dolos_stage1_mismatched.csv --set-version <firmware_version>
```

This will show you how many devices have been successfully updated and will produce a `dolos_stage1_mismatched.csv` file with any devices that have not been updated. You can use this mismatched file to re-run the repair command:

```bash
./dolos_fleet_rollout.py --csv-file dolos_stage1_mismatched.csv --repair-only
```

## 4. Subsequent Stages

If the initial stage is successful and no issues are found, you can proceed with the next stage (`dolos_stage2.csv`), and then finally with all devices (`all_dolos_devices.csv`).

## 5. Rollback

If you encounter any issues, you can roll back the firmware update by setting the firmware version to a previous stable version and scheduling a repair. You can use the same staged CSV files for a controlled rollback.

```bash
./dolos_fleet_rollout.py --csv-file dolos_stage1.csv --set-version <stable_firmware_version> --repair
```

## 6. Final steps

After FW roll out is finished and new FW confirmed to be stable, we should mark new FW as new DEFAULT.

1. For Diagnoseme to pick up new default we should add a special tag in [cloud bucket](https://pantheon.corp.google.com/storage/browser/dolos-firmware/box_firmware?pageState=(%22StorageObjectListTable%22:(%22f%22:%22%255B%255D%22))&e=-13802955&inv=1&invt=Abx_NQ&mods=component_inspector&project=chromeos-hw-tools). To do that please go to previous default FW directory and use "..." menu command "Move" on file called "default" and move it to directory with a new FW.
2. Send PSA to all Dolos developers/users that may use dolos out of fleet automation to manually pick up new changes by flashing their devices with doloscmd.

## Disclaimer
- Resources in this directory are mostly AI generated
- This script and instructions are provided as-is and may require modifications to fit your specific use case.
- Always exercise caution when performing firmware updates on production devices.
- Ensure you understand what you are doing and have recovery plan in place.
