// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
TI-TXT File Format

The TI-TXT file format used by the tool is shown as follows:
@ADDR1
D00 D01 D02 D03 D04 D05 D06 D07 D08 D09 D10 D11 D12 D13 D14 D15
D16 D17 D18 D19 D20 D21 D22 D23 D24 D25 D26 D27 D28 D29 D30 D31
...
... Dn
@ADDR2
D00 D01 D02 D03 D04 D05 D06 D07 D08 D09 D10 D11 D12 D13 D14 D15
D16 D17 D18 D19 D20 D21 D22 D23 D24 D25 D26 D27 D28 D29 D30 D31
...
... Dn
...
q
---------------------------------------------------------------
Whereas:@ADDR is the start address of a section (hexadecimal)
Dxx represents a data byte (hexadecimal)
q is the termination of the file

Notes:
1. The start address must be even.
2. Each line must have 16 data bytes, except the last line of a section.
3. Data bytes are separated by a single space.
4. The termination tag q indicates end-of-file is mandatory.
 **/
use std::fs::File;
use std::io::{self, BufRead};
use std::process::exit;

use anyhow::{Context, Result};
use log::{debug, trace, error};

const FLASH_ALIGN_SIZE: u32 = 8;
const FLASH_MAX_SIZE: u32 = 128 * 1024;
#[derive(Clone, Debug, Default)]
pub struct TiSection {
    pub address: u32,
    pub data: Vec<u8>,
}

#[derive(Default)]
pub struct TiTxt {
    pub sections: Vec<TiSection>,
}

/// File end(or end of all sections) is marked by a line with "q"
fn is_end_of_sections(line: &str) -> bool {
    line == "q"
}

/// Start of a section is marted with "@"
fn is_start_of_section(line: &str) -> bool {
    line.starts_with('@')
}

/// A section end is deduced by check if a new section starts or end of file.
fn is_end_of_previous_section(line: &str) -> bool {
    is_end_of_sections(line) || is_start_of_section(line)
}

/// Parse address line in the form of @XXXX
fn parse_address(line: &str) -> Result<u32> {
    let line = &line[1..];
    Ok(u32::from_str_radix(line, 16)?)
}

/// Parse data line in the form of D00 D01 D02 D03 D04 D05 D06 D07 D08 D09 D10 D11 D12 D13 D14 D15
fn parse_data(line: &str) -> Result<Vec<u8>> {
    let line = line.trim();
    let parts = line.split(' ');
    let digits: Vec<&str> = parts.collect();
    let mut result: Vec<u8> = Vec::new();
    for digit in digits {
        let d = u8::from_str_radix(digit, 16)?;
        result.push(d)
    }
    Ok(result)
}

/// Parse a TiTxt file
pub fn parse(path: &String) -> Result<TiTxt> {
    let mut result = TiTxt::default();
    let file = File::open(path).with_context(|| format!("Failed to open input file:: {path}"))?;
    let lines = io::BufReader::new(file).lines();
    let mut start_address: u32 = 0;
    let mut cur_address: u32 = 0;
    let mut cur_data: Vec<u8> = Vec::new();
    //Process the input file line by line
    for ref line in lines.flatten() {
        trace!("Line = [{line}]");
        if is_end_of_previous_section(line) {
            if is_start_of_section(line) {
                if cur_data.is_empty() {
                    //Handle first section in file
                    cur_address = parse_address(line)?;
                    start_address = cur_address;
                    debug!("New section started for address {cur_address:#x}");
                    continue;
                }
                let new_address: u32 = parse_address(line)?;
                debug!("New section found with start address {new_address:#x}");
                if new_address == cur_address {
                    ///Merge sections if possible to minimise erase calls
                    debug!("Section can be merged with previous section");
                    continue;
                }
            }
            //Only get here if end of section and cannot be merged with previous section
            debug!(
                "Section end reached for address {start_address:#x} size {}",
                cur_data.len()
            );
            //Pad section if necessary to write block size
            let pre_pad_count = start_address % FLASH_ALIGN_SIZE;
            start_address = start_address - pre_pad_count;
            assert!(pre_pad_count < FLASH_ALIGN_SIZE);
            if pre_pad_count != 0 {
                let zero_vec = vec![0u8; pre_pad_count as usize];
                cur_data.splice(0..0, zero_vec);
            }
            debug!("Adjusted address {cur_address:#x} size {}", cur_data.len());
            let post_pad_size = (cur_data.len() as u32 % FLASH_ALIGN_SIZE) as usize;
            if post_pad_size != 0 {
                let mut zero_vec = vec![0u8; (FLASH_ALIGN_SIZE as usize - post_pad_size) as usize];
                cur_data.append(&mut zero_vec);
            }
            if start_address + cur_data.len() as u32 > FLASH_MAX_SIZE {
                error!("Section in input file has end address outside flash memory");
                exit(1);
            }
            result.sections.push(TiSection {
                address: start_address,
                data: cur_data.clone(),
            });
            start_address = 0;
            cur_data.clear();
        }
        if is_end_of_sections(line) {
            //End of processing
            debug!("End of sections reached");
            break;
        } else if is_start_of_section(line) {
            // Only get here if is start and cannot be merged with previous section
            cur_address = parse_address(line)?;
            start_address = cur_address;
            debug!("New section started for address {cur_address:#x}");
        } else {
            //Process data line
            let mut data = parse_data(line)?;
            cur_address += data.len() as u32;
            trace!("Parsed {} hex bytes", data.len());
            cur_data.append(&mut data);
        }
    }

    Ok(result)
}
