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

use anyhow::{Context, Result};
use log::{debug, trace};

const FLASH_ALIGN_SIZE: u32 = 8;

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
    let mut cur_address: u32 = 0;
    let mut cur_data: Vec<u8> = Vec::new();
    for ref line in lines.flatten() {
        trace!("Line = [{line}]");
        if is_end_of_previous_section(line) {
            if cur_data.is_empty() {
                continue;
            }
            debug!(
                "Section end reached for address {cur_address:#x} size {}",
                cur_data.len()
            );
            let post_pad_size =
                (FLASH_ALIGN_SIZE - (cur_data.len() as u32 % FLASH_ALIGN_SIZE)) as usize;
            if post_pad_size != 0 {
                let mut zero_vec = vec![0u8; post_pad_size];
                cur_data.append(&mut zero_vec);
            }
            let pre_pad_count = cur_address % FLASH_ALIGN_SIZE;
            cur_address = cur_address - pre_pad_count;
            assert!(pre_pad_count < FLASH_ALIGN_SIZE);
            if pre_pad_count != 0 {
                let mut zero_vec = vec![0u8; pre_pad_count as usize];
                cur_data.append(&mut zero_vec);
            }
            debug!("Adjusted address {cur_address:#x} size {}", cur_data.len());

            result.sections.push(TiSection {
                address: cur_address,
                data: cur_data.clone(),
            });
            cur_address = 0;
            cur_data.clear();
        }
        if is_end_of_sections(line) {
            debug!("End of sections reached");
            break;
        } else if is_start_of_section(line) {
            cur_address = parse_address(line)?;
            trace!("New section started for address {cur_address}");
        } else {
            let mut data = parse_data(line)?;
            trace!("Parsed {} hex bytes", data.len());
            cur_data.append(&mut data)
        }
    }

    Ok(result)
}
