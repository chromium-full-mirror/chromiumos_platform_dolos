// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Dolos config reader/programmer.

use std::io::Write;
use std::ptr::read_unaligned;

mod eeprom;

use anyhow::Result;
use clap::{Parser, Subcommand};
use log::{debug, error, info};

const DEFAULT_CONFIG_FILE_NAME: &str = "dolos_sb_config.bin";

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
#[command(propagate_version = true)]
#[clap(author, verbatim_doc_comment)]
/// Extracts config from smart battery over SMBus(through USB bridge) and then saves it as binary file.
/// The binary file can be viewed in human readable format using "print" command or
/// can be programmed on to one or more Dolos device(s).
struct Cli {
    #[command(subcommand)]
    command: Command,
    /// Logging verbosity level.
    ///   0(default)=>Silent. 1=>`info` messages. 2=>`debug` messages too.
    #[clap(long, short, global = true, default_value_t = 1)]
    verbose: usize,
}

#[derive(Subcommand)]
enum Command {
    /// Extract Smart Battery config by reading through SMBus and store it as binary file
    Extract {
        /// Destination file name where the read config should be stored.
        #[clap(default_value=DEFAULT_CONFIG_FILE_NAME)]
        file_name: String,

        /// USB to i2c bridge address in /dev node. Run `i2cdetect -l` to find out.
        #[clap(long, default_value_t = 16)]
        usb_i2c_bridge_address: u16,

        /// Polarity of the System Present Signal to be stored in the EEPROM.
        #[clap(long, default_value_t = 1)]
        polarity: u8,

        /// Dont read SmartBattery extension registers.
        #[clap(long, short)]
        no_extension: bool,
    },

    /// Print Smart Battery config stored in a file.
    Print {
        /// Config file path which was extracted previously by extract command.
        #[clap(default_value=DEFAULT_CONFIG_FILE_NAME)]
        file_name: String,
    },
}

/// Convert given log level to name string.
fn log_level_to_string(level: log::Level) -> &'static str {
    match level {
        log::Level::Info => "INFO",
        log::Level::Debug => "DEBUG",
        _ => "ERROR",
    }
}

/// Initialize logging with specified verbosity level.
fn init_logger(verbose: usize) {
    let level = match verbose {
        0 => log::LevelFilter::Error,
        1 => log::LevelFilter::Info,
        _ => log::LevelFilter::Debug,
    };

    env_logger::Builder::new()
        .filter_level(level)
        .format(|buf, record| {
            let style = buf.default_level_style(record.level());
            writeln!(
                buf,
                "{:5} {:>10}:{:<5} {}",
                style.value(log_level_to_string(record.level())),
                record.file().unwrap_or("unknown"),
                record.line().unwrap_or(0),
                record.args()
            )
        })
        .init();
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    init_logger(cli.verbose);

    match &cli.command {
        Command::Extract {
            file_name,
            polarity,
            usb_i2c_bridge_address,
            no_extension,
        } => {
            debug!(
                "Parsed arguments: file:{file_name} i2c_dev_address:{usb_i2c_bridge_address:#x}"
            );
            match eeprom::read_sb_registers(*usb_i2c_bridge_address, *no_extension) {
                Ok(config) => {
                    info!("{config}");
                    eeprom::write_eeprom_layout_to_file(file_name, *polarity, &config)?;
                }
                Err(e) => {
                    error!("failed to read battery config - {e}")
                }
            }
        }

        Command::Print { file_name } => {
            debug!("Loading {file_name}");
            let (raw_eeprom_data, config) = eeprom::read_sb_registers_set_from_file(file_name)?;
            println!("Version:     {}", raw_eeprom_data.version);
            println!(
                "Date:        {}/{}",
                raw_eeprom_data.manufactured_week, raw_eeprom_data.manufactured_year
            );
            println!("Polarity:    {}", raw_eeprom_data.polarity);
            unsafe {
                println!(
                    "Serial:      {}",
                    read_unaligned(std::ptr::addr_of!(raw_eeprom_data.serial_number))
                );
            }
            unsafe {
                println!(
                    "CRC:         {:#x}",
                    read_unaligned(std::ptr::addr_of!(raw_eeprom_data.crc))
                );
            }
            print!("Smart Battery Registers:");
            println!("{config}");
        }
    }
    Ok(())
}
