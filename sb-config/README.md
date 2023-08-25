`sb-config` is a tool which reads smart battery config through usb to i2c bridge.
The read config can be sent to dolos firmware through UART.

## Command line

```
Usage: sb-config [OPTIONS] <COMMAND>

Commands:
  extract  Extract Smart Battery config by reading through SMBus and store it as binary file
  print    Print Smart Battery config stored in a file
  program  Read Smart Battery config stored in a file and write to Dolos
  help     Print this message or the help of the given subcommand(s)

Options:
  -v, --verbose <VERBOSE>  Logging verbosity level. 0(default)=>Silent. 1=>`info` messages. 2=>`debug` messages too [default: 1]
  -h, --help               Print help
  -V, --version            Print version

```
## Example Usage
```
sb-config extract --usb-i2c-bridge-address 15
sb-config program --usb-uart-path /dev/ttyACM1
```
# Setup

## Extracting

To extract the config from a battery, sb-config requires usb to i2c bridge like shown in the below diagram.
```
    ┌───────────────┐
    │               │
    │     sb-config │
    │   application │
    ├───────────────┤
    │    i2c-dev    │Linux PC
    │     module    │
    └──────┬┬───────┘
           ││
           ││ USB Cable
           ││
      ┌────┴┴─────┐
      │USB to i2c │ Eg: CP2112-EK
      │  bridge   │
      └────┬┬─────┘
           ││
           ││
           ││ i2c cable/jumper
           ││
      ┌────┴┴─────┐
      │    i2c    │
      │   device  │SMART Battery
      └───────────┘


```

## Programming
To program, dolos UART should be connected to the host machine.
```
     ┌───────────────┐
     │               │
     │     sb-config │
     │   application │
     ├───────────────┤
     │               │Linux PC
     │      uart     │
     └──────┬┬───────┘
            ││
            ││ USB Cable
            ││
       ┌────┴┴─────┐
       │USB to uart│  FTDI
       │  bridge   │
       └────┬┬─────┘
            ││
            ││       UART
            ││
            ││
       ┌────┴┴─────┐
       │   UART    │
       │   client  │ Dolos
       └───────────┘

```

# Debugging

i2c-detect should list usb2i2c bridge.
If i2c-detect is not installed then try the following:

```
sudo apt install i2c-tools
i2c-detect -l
```

Verify i2cdev kernel module is loaded
```
sudo modprobe i2c-dev
```
