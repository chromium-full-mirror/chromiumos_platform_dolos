## Overview
This tool was designed to replace the `tiarmhex` utility in the build flow of the Dolos firmware and convert binary files to TI-TXT binaries.

### TI-TXT Format
The TI-TXT hex format is the format used for flashing binaries to the Dolos. It supports 16-bit hexadecimal data and consists of section start addresses, data byte, and an end-of-file character. More details can be found [here](https://downloads.ti.com/docs/esd/SPRUI03/ti-txt-hex-format-ti-txt-option-stdz0795656.html).

### Usage
This tool is integrated in the build flow for the firmware but can also be run manually as follows:
```
python3 converter.py [-s SECTION_START] input_file output_file
```
where SECTION_START is the starting address of the binary, provided in hexadecimal format. It defaults to `0x0000`

e.g.
```
python3 converter.py -s 0x8000 zephyr.bin zephyr.txt
```

## Running tests

To run the tests you need to have the `pytest` installed. Running the tests then can be done from dolos/tools/formatconverter

```
python3 -m pytest tests
```
