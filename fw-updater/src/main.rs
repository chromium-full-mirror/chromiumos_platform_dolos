// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::io::Write;
use std::thread::sleep;
use std::time::Duration;

use anyhow::{bail, Result};
use clap::Parser;
use log::{debug, info, trace};

use fw_updater::bsl;
use fw_updater::titxt;
use fw_updater::uart::Uart;
use fw_updater::{MAUI_PID, MAUI_VID};

/// Program the MSPM0G MCU firmware (in Ti TXT format) through UART or direct USB DFU.
#[derive(Parser, Debug)]
#[clap(author, version=env!("VERSION"), verbatim_doc_comment)]
pub struct Args {
    /// Dolos or Maui UART port through which initial BSL jump command is sent.
    /// If omitted in standard UART mode, defaults to /dev/ttyUSB0.
    /// If omitted in USB mode (--usb), auto-detects Maui CDC ACM serial interfaces.
    #[clap(short, long)]
    uart: Option<String>,

    /// Target Maui onboard USB Hub serial number (e.g. CC00086185C0).
    /// Used to disambiguate multiple Maui devices in USB DFU or normal mode.
    #[clap(long = "hub-serial")]
    hub_serial: Option<String>,

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

    /// Use direct USB interface instead of UART for BSL programming
    #[clap(long, default_value = "false", action, verbatim_doc_comment)]
    usb: bool,

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

fn flash_image<T: bsl::Transport>(bsl: &mut bsl::Bsl<T>, image: &titxt::TiTxt) -> Result<()> {
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
    Ok(())
}

pub fn program_usb(image: &titxt::TiTxt, location: Option<&fw_updater::usb::UsbLocation>) -> Result<()> {
    let usb = fw_updater::usb::Usb::open(0x2047, 0x0210, location)?;
    let mut bsl = bsl::Bsl::open(usb)?;

    bsl.connect()?;
    info!("connected to BSL");

    bsl.unlock_bootloader()?;
    trace!("Unlocked bootloader");

    let bsl_info = bsl.get_device_info()?;
    info!("bsl_info: {bsl_info:?}");

    flash_image(&mut bsl, image)?;

    bsl.start_application()?;
    info!("Firmware started");

    Ok(())
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

    flash_image(&mut bsl, image)?;

    bsl.start_application()?;
    info!("Firmware started");

    Ok(())
}

fn verify_serial_port_vid_pid(port_path: &str) -> Result<()> {
    let Ok(ports) = serialport::available_ports() else {
        return Ok(());
    };

    for p in ports {
        if p.port_name != port_path {
            continue;
        }

        if let serialport::SerialPortType::UsbPort(info) = p.port_type {
            if info.vid == MAUI_VID && info.pid == MAUI_PID {
                return Ok(());
            }
            bail!(
                "Specified port {} ({:04x}:{:04x}) is not a Maui CDC ACM device ({:04x}:{:04x}). When using --usb, please specify a Maui USB device or omit --usb for standard UART flashing.",
                port_path, info.vid, info.pid, MAUI_VID, MAUI_PID
            );
        }
    }
    Ok(())
}

fn main() -> Result<()> {
    let args = Args::parse();
    init_logger(args.verbose);

    info!("Reading image: {}", args.firmware_image);
    let image = titxt::parse(&args.firmware_image)?;

    let mut usb_location = args.hub_serial.as_ref().map(|hub_sn| fw_updater::usb::UsbLocation {
        bus: 0,
        ports: Vec::new(),
        parent_hub_serial: Some(hub_sn.clone()),
        port_on_parent: None,
    });

    if let Some(ref hub_sn) = args.hub_serial {
        info!("Target device USB hub serial filter: {hub_sn}");
    }

    if !args.bsl_mode {
        // Determine the initial UART port:
        // 1. Explicitly provided via --uart <PORT>.
        // 2. In USB mode (--usb), auto-detected via Maui CDC ACM VID:PID (18d1:5085), optionally filtered by --hub-serial.
        // 3. In standard UART mode, defaults to "/dev/ttyUSB0".
        let uart_path = if let Some(ref port) = args.uart {
            if args.usb {
                verify_serial_port_vid_pid(port)?;
            }
            port.clone()
        } else if args.usb {
            let port = Uart::find_serial_port(MAUI_VID, MAUI_PID, args.hub_serial.as_deref())?;
            info!("Auto-detected Maui device serial port: {port}");
            port
        } else {
            "/dev/ttyUSB0".to_string()
        };

        // Capture physical USB hub topology location (bus and port hierarchy) from the UART path.
        // This ensures subsequent USB DFU or UART re-enumeration connects to the exact same physical device.
        let loc = fw_updater::usb::UsbLocation::from_tty_path(&uart_path);
        if let Some(ref l) = loc {
            if let Some(ref hub_sn) = l.parent_hub_serial {
                info!("Target device USB physical location: parent hub serial {}, bus {} ports {:?}", hub_sn, l.bus, l.ports);
            } else {
                info!("Target device USB physical location: bus {} ports {:?}", l.bus, l.ports);
            }
            usb_location = loc;
        }

        let mut uart = Uart::open(&uart_path, args.fw_baud_rate)?;
        debug!("Clearing UART IO buffers");
        uart.clear_io()?;

        info!("Sending BSL command to device at {uart_path}");
        jump_to_bsl(&mut uart, "bsl\n")?;
        drop(uart);

        if args.usb {
            info!("Waiting for device to re-enumerate as TI DFU...");
            sleep(Duration::from_secs(1));
        }
    } else if let Some(ref port) = args.uart {
        usb_location = fw_updater::usb::UsbLocation::from_tty_path(port);
    }

    info!("Programming the device");
    if args.usb {
        program_usb(&image, usb_location.as_ref())?;
    } else {
        let uart_path = args.uart.unwrap_or_else(|| "/dev/ttyUSB0".to_string());
        program(&uart_path, args.bsl_baud_rate, &image)?;
    }

    info!("Completed successfully");

    Ok(())
}
