// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::io::Write;
use std::thread::sleep;
use std::time::Duration;
use std::process::exit;

use anyhow::Result;
use clap::Parser;
use log::{debug, info, trace, error};
use regex::Regex;

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
        0 => log::LevelFilter::Info,
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

const APP_FIRMWARE_DELAY_MSEC: u64 = 500;

// Gets the firmware version
fn firmware_version(uart: &mut Uart) -> Result<String> {
    info!("Obtaining firmware version");
    // Send version command
    let mut version_buf = vec![0u8; 128];
    uart.write_all("version\r".as_bytes())?;
    sleep(Duration::from_millis(APP_FIRMWARE_DELAY_MSEC));
    uart.read_all(&mut version_buf)?;

    ///Check if was able to read version
    if version_buf.iter().all(|&x| x == 0) {
        error!("Unable to read from UART. Maybe another process is holding the connection?");
        exit(1);
    }

    // Get the version from the output string
    let version_str = String::from_utf8(version_buf).unwrap();
    let re = Regex::new(r"[0-9]+\.[0-9]+(\.[0-9])?-[a-z0-9]+").unwrap();
    let m = re.find(&version_str).unwrap();
    info!("Firmware version: {}", &version_str[m.start()..m.end()]);

    Ok(version_str[m.start()..m.end()].to_string())
}

/// Jump to BSL from firmware.
fn jump_to_bsl(uart: &mut Uart, cmd: &str) -> Result<()> {
    info!("Jumping to BSL");
    let cmd_bytes = cmd.as_bytes();
    debug!("Sending firmware command to enter BSL");
    uart.write_all(cmd_bytes)?;
    debug!("Waiting for {APP_FIRMWARE_DELAY_MSEC} milliseconds for bootloader to come backup");
    sleep(Duration::from_millis(APP_FIRMWARE_DELAY_MSEC));

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
    info!("connected to BSL");

    bsl.unlock_bootloader()?;
    trace!("Unlocked bootloader");

    let bsl_info = bsl.get_device_info()?;
    info!("bsl_info: {bsl_info:?}");

    bsl.change_baud_rate(baud_rate)?;
    let uart = Uart::open(path, baud_rate)?;
    let mut bsl = bsl::Bsl::open(uart)?;

    for section in &image.sections {
        //Last byte in section start address is start + len - 1
        let end_address: u32 = (section.address + section.data.len() as u32) - 1;
        bsl.erase(section.address, end_address)?;
        info!("Successfully erased flash from {0:#x} to {end_address:#x}", section.address);
        trace!(
            "Programming section @{:#x} with {}bytes",
            section.address,
            section.data.len()
        );
        bsl.program(section.address, section.data.clone(), &progress)?;
    }

    bsl.start_application()?;
    info!("Firmware started");

    Ok(())
}

fn main() -> Result<()> {
    let args = Args::parse();
    init_logger(args.verbose);

    info!("Reading image: {}", args.firmware_image);
    let image = titxt::parse(&args.firmware_image)?;

    if !args.bsl_mode {
        let mut uart = Uart::open(&args.uart, args.fw_baud_rate)?;
        debug!("Clearing UART IO buffers");
        uart.clear_io()?;

        let version = firmware_version(&mut uart).unwrap();

        // Check version number, 0.70 will be considered baremetal
        if version.contains("0.70") {
            info!("Firmware is running on baremetal");
            jump_to_bsl(&mut uart, "jump-to-bsl\r")?;
        } else {
            info!("Firmware is running on Zephyr OS");
            jump_to_bsl(&mut uart, "bsl\n")?;
        }
    }

    info!("Programming the device");
    program(&args.uart, args.bsl_baud_rate, &image)?;

    info!("Completed successfully");

    Ok(())
}
