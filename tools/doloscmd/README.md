## Overview / Usage

This set of command is designed as an abstraction between the test infrastructure and the Dolos serial console, to provide a strong contract between the two and allow changes in the console to not impact users.

The commands return json serialized version of the response objects detailed in
doloscmd.proto

The commands will be:

```
doloscmd version: returns the version of the dolos firmware

arguments:  --serial <dolos serial number> |  --uartname <dolos uart serial number>

returns <GetVersionResponse>

max execution time:TBD
```

```
doloscmd status: returns a status code see go/cros-dolos-status

arguments:  --serial <dolos serial number> |  --uartname <dolos uart serial number>

returns <GetStatusResponse>

max execution time: TBD
```

```
doloscmd repair: dolos will do any internal reset/reboot it can to repair a failing device.

arguments:  --serial <dolos serial number> |  --uartname <dolos uart serial number>

returns <GetRepairResponse>

max execution time: TBD
```
```
doloscmd update-firmware

arguments:  --serial <dolos serial number> |  --uartname <dolos uart serial number> --firmware_file

returns <FirmwareUpdateResponse>

max execution time: 60 seconds
```
```
doloscmd find-uartname

arguments:  --serial <dolos serial number>

returns <FinduartnameResponse>

max execution time: TBD
```

Return codes are also detailed in the doloscmd.proto in the ERROR_CODE enum.


find-uartname is the only unusual command, the dolos serial number is not visible to the host machine via lsusb or other commands.  The only way to get the dolos serial number is to run a command on the dolos serial console.

To prevent having to search for the correct device on every command it was decided that infrastructure will keep a mapping from doloscmd serial number to UART name/serial.   At DUT deployment time the UART value will be populated by calling the find-uartname.   Although calling all doloscmd’s with the dolos serial number is supported, it will be relatively slow compared to calling a doloscmd with the uart serial number.

## Building

To build a wheel file for distribution

```
python3 setup.py bdist_wheel
```

## Running tests

Following the building section first - even if you do not need the whl file it generates the correct python
files from the proto.

To run the tests you need to have the following installed

```
pytest
pytest-mock
serial
```

In debian the easiest way to install these is :

```
sudo apt install python3-pytest python3-pytest-mock python3-serial
```

Running the test then can be done from the dolos/tools/doloscmd

```
python3 -m pytest tests
```
