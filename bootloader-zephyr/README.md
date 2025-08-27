# Dolos Bootloader

This is the bootloader for the Dolos device. It's the first software to run on the device and is responsible for basic hardware initialization, providing a command-line interface for recovery, and loading the main application firmware.

## Operation

On power-up, the bootloader performs the following steps:
1. **Initialization**: It initializes the UART peripheral to use the interrupt driven API and buffers TX messages in a ring buffer, in order to be able to operate without a terminal connected.
2. **Configuration check**: It attempts to open the `ro_partition` to read configuration flags. If the partition cannot be read, it proceeds with default settings.
3. **Developer Flag**: If the developer flag is enabled, it skips the user interaction timeout.
4. **User interaction**: It waits for user input, and if none is received after 10 seconds, it boots the main image.

## Configuration
The RO partition is structured in `32B` wide records. The 1st record contains the version string, mainly for the firmware to query. The 2nd record contains flags. The remaining records do not carry meaning for now.

### Flags Record
Since the erase value of the device flash is `0xFF`, all flags use negative logic, so a value of `0` means enabled and a value of `1` means disabled.
The flags record contains the following flags:
#### Developer Flag
* Position: LSB in the 1st byte of the record
* Meaning: If enabled the boot timeout is skipped, and the main image booted directly.
