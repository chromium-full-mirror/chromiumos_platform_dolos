## First Time Setup

For first time setup we need to create a virtual environment, enter it, and
install dependencies. These steps have consolidated into a start script.

```console
source start_venv.sh
```

#### Install venv if missing:

If venv is missing, install the package with:

```console
sudo apt-get install python3-venv
```

### Exit Virtual Environment

Exit the virtual environment using the `deactivate` command.

```console
(.venv) ~/chromiumos/src/third_party/dolos/battery $ deactivate
~/chromiumos/src/third_party/dolos/batterydata $
```

### Optional: Setup crosfleet and SSH access:

https://goto.google.com/crosfleet-cli
https://goto.google.com/chromeos-lab-duts-ssh

## Importing Battery Information

```console
(.venv) $ dolos_load_battery  --help
usage: dolos_load_battery [-h] (-s SSH | -i) [-p I2C_PORT] -f FILE [-c COMMENT] [-v]

options:
  -h, --help   show this help message and exit
  -s SSH       Use SSH and ECTool to extract registers from a remote DUT
  -i           Use a FTDI board to extract registers from an unplugged battery
  -p I2C_PORT  Configure ECTool's I2C port if I2C_PORT_BATTERY is known
  -f FILE      File path to export yaml data
  -c COMMENT   Comment data
  -v           Enable verbose logging
```

### Steps to extract battery data from the Fleet

Find checkout `MODEL` with crosfleet and identify the `DUT_HOSTNAME` field.
```console
crosfleet dut lease -model $MODEL
```

Example:
```console
crosfleet dut lease -model pico
Verifying the provided DUT dimensions...
Found 45 DUT(s) (39 busy) matching the provided DUT dimensions
...
DUT_HOSTNAME=chromeos8-row10-rack8-host46
MODEL=pico
BOARD=jacuzzi
SERVO_HOSTNAME=chromeos8-row10-rack8-labstation6
SERVO_PORT=9996
SERVO_SERIAL=SERVOV4P1-S-2103240648
```

Pass the `DUT_HOSTNAME` Parameter to the dolos_load_battery.py script and specify
an `OUTPUT` path for the yaml data.


```console
dolos_load_battery -s $DUT_HOSTNAME -f $OUTPUT
```

Example:
```console
(.venv) $ dolos_load_battery -s chromeos8-row10-rack8-host46 -f pico.yaml
INFO - dolos_load_battery.py:252:configure_connections : Connected
INFO - dolos_load_battery.py:176:_configure_port : Detecting port
INFO - dolos_load_battery.py:180:_configure_port : Using port 4
INFO - dolos_load_battery.py:197:load_registers : Loading registers
INFO - dolos_load_battery.py:201:load_registers : Progress 1/40 - Loading 'SB_MANUFACTURER_ACCESS'
...
INFO - dolos_load_battery.py:201:load_registers : Progress 40/40 - Loading 'SB_PACK_STATUS'
INFO - dolos_load_battery.py:209:export_table : Exporting yaml to:PosixPath('test.yaml')
```

Final steps:

PolarityHigh can not be extracted automatically. We need to test it with full
DUT-Cable-Dolos setup to confirm it is correct. It needs to be set to be set to
`low` or `high`.

```yaml
Polarity: high
```

When the DUT is powered on, we can see the correct state using the `status` command
in the Dolos console:

```console
System present     : Absent (signal HIGH, polarity LOW)
```

If the DUT is active and `signal HIGH` is reported, we need to adjust the Polarity.

```yaml
Polarity: high
```

```console
System present     : Present (signal HIGH, polarity HIGH)
```

## Programming The Cable

We can program the EEPROM in the Dolos cable using a Dolos or FTDI board like the FT4232H.
Simply pass the `YAML` file in and provide fields to specify the date, and polarity.

If the serial field is left empty, it'll preserve the existing serial.

```console
(.venv) $ dolos_program_cable --help
usage: dolos_program_cable [-h] (-y YAML | -yn YAML_NETWORK) (-d DOLOS | -du DOLOS_UART | -f) [-ns NEW_SERIAL] [-v] [-id HWID]

options:
  -h, --help        show this help message and exit
  -y YAML           File path of imported yaml data
  -yn YAML_NETWORK  Load the latest config from the storage bucket
  -d DOLOS          Use Dolos' serial to program: 'any' is used if the serial number does not matter.
  -du DOLOS_UART    Use Dolos' UART to program: This allows programming when serials can't be used.
  -f                Use FTDI to program: Fails if more than 1 device is found.
  -ns NEW_SERIAL    Optional overwrite serial number
  -v                Enable verbose logging
  -id HWID          Optional hwid field
```

Example arguments using Dolos to fetch the latest meep config
```console
(.venv) $ dolos_program_cable -yn meep -d any
```

Example arguments using Dolos with HWID using a yaml created from unit tests
```console
dolos_program_cable -y test.yaml -d any -id "MODEL A2B-A2A-B2G"
```

Example using Dolos:
```console
(.venv) $ dolos_program_cable -y pico.yaml -d any
INFO - dolos_program_cable.py:30:load_table : Loading yaml: PosixPath('pico.yaml')
INFO - dolos_program_cable.py:113:read : Reading cable
INFO - dolos_program_cable.py:132:read : Serial 'DOLOSV187-C-120251234'
INFO - dolos_program_cable.py:137:write : Programming cable
INFO - dolos_program_cable.py:142:write : Verifying cable
INFO - dolos_program_cable.py:113:read : Reading cable
INFO - dolos_program_cable.py:132:read : Serial 'DOLOSV187-C-120251234'
INFO - dolos_program_cable.py:145:write : Program success
```

```console
(.venv) $ dolos_program_cable -y pico.yaml -d DOLOSV3-C-3320241234
```

Example using FTDI:
```console
(.venv) $ dolos_program_cable -y pico.yaml -f
```
