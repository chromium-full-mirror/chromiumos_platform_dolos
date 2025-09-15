## Quick setup

The setup uses 2 Servos and 1 Dolos.
* `servo_main`:
  * Top USB port used for Dolos host cable
  * Servo cable used for DUT power
* `servo_power`:
  * Servo cable used for Dolos power

Run `./start-tests.sh SERIAL_SERVO_MAIN SERIAL_SERVO_POWER TARGET_FW_VERSION`
with the correct serial numbers of the servos and with the desired target version for the firmware.

**NOTE**: Minimum version is `1.228.0-ced081e`. You must provide the entire version string.
For now the version string can have any of the major versions `1.` `2.` `3.`
however in the future the intent is that the version major also indicates what testing you intend to run:
- `1.`: firmware version -- will run tests centered around the firmware
- `2.`: bootloader version -- will run tests centered around the bootloader
- `3.`: combined version -- will run all tests

If you encounter any issues check the following:
- Is any other process holding the serial connection?
- Do you have more than one Dolos device connected?
- Retry, sometimes USB devices don't enumerate properly on the first run.
