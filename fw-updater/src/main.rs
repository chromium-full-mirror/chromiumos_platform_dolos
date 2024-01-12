// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::io::Write;
use std::thread::sleep;
use std::time::Duration;

use anyhow::Result;
use clap::Parser;
use log::{debug, info, trace};

use fw_updater::bsl;
use fw_updater::titxt;
use fw_updater::uart::Uart;

/// Dolos firmware updater.
///
/// Program the Dolos firmware(in Ti TXT format) through UART.
#[derive(Parser, Debug)]
#[clap(author, version=env!("VERSION"), verbatim_doc_comment)]
pub struct Args {
    /// Dolos UART port through which the programming will be done.
    #[clap(short, long, default_value = "/dev/ttyUSB0")]
    uart: String,

    /// FW UART baudrate.
    #[clap(short, long, default_value = "115200")]
    fw_baud_rate: u32,

    /// BSL UART baudrate.
    /// BSL always starts at 9600 but we can upgrade to new baud rate after connection.
    #[clap(short, long, default_value = "115200")]
    bsl_baud_rate: u32,

    /// Path to firmware image in Ti TXT format.
    #[clap()]
    firmware_image: String,

    /// Assume the device already in BSL mode and dont issue jump-to-bsl command.
    /// By default the programmer will issue jump-to-bsl command and to make the device to go
    /// into bsl mode before starting the programming
    #[clap(long, default_value = "false", action, verbatim_doc_comment)]
    bsl_mode: bool,

    /// Logging verbosity level.
    /// By default only error messages are logged.
    /// Pass multiple -v times to increase the verbosity.
    ///   -v         => Print `warning` messages too.
    ///   -vv        => Print `info` messages too.
    ///   -vvv       => Print `debug` messages too.
    #[clap(short, long, parse(from_occurrences), verbatim_doc_comment)]
    verbose: usize,
}

/// Convert given log level to name string.
fn log_level_to_string(level: log::Level) -> &'static str {
    match level {
        log::Level::Error => "ERROR",
        log::Level::Warn => "WARN",
        log::Level::Info => "INFO",
        log::Level::Debug => "DEBUG",
        log::Level::Trace => "TRACE",
    }
}

/// Initialize logging with specified verbosity level.
fn init_logger(verbose: usize) {
    let level = match verbose {
        0 => log::LevelFilter::Error,
        1 => log::LevelFilter::Warn,
        2 => log::LevelFilter::Info,
        3 => log::LevelFilter::Debug,
        _ => log::LevelFilter::Trace,
    };

    env_logger::Builder::new()
        .filter_level(level)
        .format(|buf, record| {
            let style = buf.default_level_style(record.level());
            writeln!(
                buf,
                "{:5} {:>10}:{:<3} {}",
                style.value(log_level_to_string(record.level())),
                record.file().unwrap().split('/').nth(1).unwrap(),
                record.line().unwrap_or(0),
                record.args()
            )
        })
        .init();
}

const APP_FIRMWARE_DELAY_SEC: u64 = 3;

/// Jump to BSL from firmware.
fn jump_to_bsl(uart: &mut Uart) -> Result<()> {
    // flush any command
    uart.write_all("\r".as_bytes())?;
    sleep(Duration::from_secs(APP_FIRMWARE_DELAY_SEC));
    let cmd = "jump-to-bsl\r";
    let cmd_bytes = cmd.as_bytes();
    debug!("Sending firmware command to enter BSL");
    uart.write_all(cmd_bytes)?;
    debug!("Waiting for {APP_FIRMWARE_DELAY_SEC} seconds for bootloader to come backup");
    sleep(Duration::from_secs(APP_FIRMWARE_DELAY_SEC));

    Ok(())
}

fn progress(total: u32, completed: u32) {
    println!("{:3.0} %", (completed as f32 / total as f32) * 100f32);
}

pub fn program(path: &String, baud_rate: u32, image: &titxt::TiTxt) -> Result<()> {
    //By default BSL starts with 9600 baudrate.
    let uart = Uart::open(path, 9600)?;
    let mut bsl = bsl::Bsl::open(uart)?;

    bsl.connect()?;
    debug!("connected to BSL");

    bsl.unlock_bootloader()?;
    trace!("Unlocked bootloader");

    let bsl_info = bsl.get_device_info()?;
    info!("bsl_info: {bsl_info:?}");

    bsl.change_baud_rate(baud_rate)?;
    let uart = Uart::open(path, baud_rate)?;
    let mut bsl = bsl::Bsl::open(uart)?;

    bsl.erase()?;
    debug!("Successfully erased flash");

    for section in &image.sections {
        trace!(
            "Programming section @{:#x} with {}bytes",
            section.address,
            section.data.len()
        );
        bsl.program(section.address, section.data.clone(), &progress)?;
    }

    bsl.start_application()?;
    debug!("Firmware started");

    Ok(())
}

fn main() -> Result<()> {
    let args = Args::parse();
    init_logger(args.verbose);

    info!("Reading image: {}", args.firmware_image);
    let image = titxt::parse(&args.firmware_image)?;

    if !args.bsl_mode {
        info!("Jumping to BSL");
        let mut uart = Uart::open(&args.uart, args.fw_baud_rate)?;
        jump_to_bsl(&mut uart)?;
    }

    info!("Programming the device");
    program(&args.uart, args.bsl_baud_rate, &image)?;

    info!("Completed successfully");

    Ok(())
}
