// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Methods to send SMBus config to FW through UART.

use std::fs::File;
use std::io::{BufRead, BufReader, Write};
use std::time::Duration;

use anyhow::{bail, Result};
use log::{debug, info};
use serialport::TTYPort;

use crate::smart_battery::{SBConfig, SBRegisterValue};

const UART_BAUD_RATE: u32 = 9600;
const UART_TIMEOUT_MS: u64 = 2000;

const UART_SBREG_CMD: &str = "sbreg";
const UART_CMD_END_SIG: &str = "$\n";

fn open_serial_port(uart_path: &String) -> Result<TTYPort> {
    let mut serial_port = match serialport::new(uart_path, UART_BAUD_RATE)
        .timeout(Duration::from_millis(UART_TIMEOUT_MS))
        .open_native()
    {
        Ok(v) => v,
        Err(e) => bail!("Failed to open UART {uart_path}: {e}"),
    };
    serial_port.set_exclusive(false)?;
    Ok(serial_port)
}

/// Send all registers stored in a config file to FW
pub fn program_config(file_name: &String, uart_path: &String) -> Result<()> {
    let mut f = File::open(file_name)?;
    let config: SBConfig = bincode::deserialize_from(&mut f)?;

    let mut write_serial_port = open_serial_port(uart_path)?;
    let read_serial_port = open_serial_port(uart_path)?;
    let mut serial_reader = BufReader::new(read_serial_port);

    for (index, reg) in config.registers.iter().enumerate() {
        let (reg_type, reg_len, reg_value) = match &reg.value {
            SBRegisterValue::BlockData(v) => {
                let str_vec: Vec<String> = v.iter().map(|n| n.to_string()).collect();
                let value = str_vec.join(" ");
                ("B", v.len(), value)
            }
            SBRegisterValue::Word(v) => {
                let first = v >> 8;
                let second = v & 0xFF;
                let value = format!("{first} {second}");
                ("W", 2, value)
            }
            SBRegisterValue::Reserved => ("R", 0, "".to_string()),
        };
        let cmd =
            format!("{UART_SBREG_CMD} {index} {reg_type} {reg_len} {reg_value} {UART_CMD_END_SIG}");
        let cmd_bytes = cmd.as_bytes();
        info!("Writing to UART: \'{}\'", cmd.trim());
        let written_len = write_serial_port.write(cmd_bytes)?;
        if written_len != cmd_bytes.len() {
            bail!("partially wrote cmd {cmd:?}");
        }
        write_serial_port.flush()?;

        debug!("Waiting for results from FW");
        let mut result = String::new();
        serial_reader.read_line(&mut result).unwrap();
        let result = result.trim();
        info!("Result: \'{result}\'");
    }

    Ok(())
}
