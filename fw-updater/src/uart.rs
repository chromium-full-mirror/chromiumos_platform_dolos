// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::os::fd::AsRawFd;
use std::path::Path;
use std::thread::sleep;
use std::time::Duration;

use anyhow::{bail, Result};
use log::{info, trace};
use termios::os::linux::CRTSCTS;
use termios::*;
use std::io::{Error, ErrorKind};
use regex::Regex;
use crate::bsl::Transport;

impl Transport for Uart {
    fn write_all(&mut self, data: &[u8]) -> Result<()> {
        self.write_all(data)
    }
    fn read_exact(&mut self, buf: &mut [u8]) -> Result<()> {
        self.read_exact(buf)
    }
    fn flush_input(&mut self) -> Result<()> {
        self.flush_input()
    }
}


pub struct Uart {
    file: File,
}

fn get_uart_speed(baud_rate: u32) -> Result<speed_t> {
    if baud_rate == 0 {
        return Ok(termios::os::linux::B0);
    }
    if baud_rate == 50 {
        return Ok(termios::os::linux::B50);
    }
    if baud_rate == 75 {
        return Ok(termios::os::linux::B75);
    }
    if baud_rate == 110 {
        return Ok(termios::os::linux::B110);
    }
    if baud_rate == 134 {
        return Ok(termios::os::linux::B134);
    }
    if baud_rate == 150 {
        return Ok(termios::os::linux::B150);
    }
    if baud_rate == 200 {
        return Ok(termios::os::linux::B200);
    }
    if baud_rate == 300 {
        return Ok(termios::os::linux::B300);
    }
    if baud_rate == 600 {
        return Ok(termios::os::linux::B600);
    }
    if baud_rate == 1200 {
        return Ok(termios::os::linux::B1200);
    }
    if baud_rate == 1800 {
        return Ok(termios::os::linux::B1800);
    }
    if baud_rate == 2400 {
        return Ok(termios::os::linux::B2400);
    }
    if baud_rate == 4800 {
        return Ok(termios::os::linux::B4800);
    }
    if baud_rate == 9600 {
        return Ok(termios::os::linux::B9600);
    }
    if baud_rate == 19200 {
        return Ok(termios::os::linux::B19200);
    }
    if baud_rate == 38400 {
        return Ok(termios::os::linux::B38400);
    }
    if baud_rate == 57600 {
        return Ok(termios::os::linux::B57600);
    }
    if baud_rate == 115200 {
        return Ok(termios::os::linux::B115200);
    }
    if baud_rate == 230400 {
        return Ok(termios::os::linux::B230400);
    }
    if baud_rate == 460800 {
        return Ok(termios::os::linux::B460800);
    }
    if baud_rate == 500000 {
        return Ok(termios::os::linux::B500000);
    }
    if baud_rate == 576000 {
        return Ok(termios::os::linux::B576000);
    }
    if baud_rate == 921600 {
        return Ok(termios::os::linux::B921600);
    }
    if baud_rate == 1000000 {
        return Ok(termios::os::linux::B1000000);
    }
    if baud_rate == 1152000 {
        return Ok(termios::os::linux::B1152000);
    }
    if baud_rate == 1500000 {
        return Ok(termios::os::linux::B1500000);
    }
    if baud_rate == 2000000 {
        return Ok(termios::os::linux::B2000000);
    }
    if baud_rate == 2500000 {
        return Ok(termios::os::linux::B2500000);
    }
    if baud_rate == 3000000 {
        return Ok(termios::os::linux::B3000000);
    }
    if baud_rate == 3500000 {
        return Ok(termios::os::linux::B3500000);
    }
    if baud_rate == 4000000 {
        return Ok(termios::os::linux::B4000000);
    }

    bail!("Invalid baud rate: {baud_rate}");
}

const FTDI_VID: u16 = 0x0403;

fn try_open_reenumerated_port(usb_loc: Option<&crate::usb::UsbLocation>) -> Option<(String, File)> {
    let ports = serialport::available_ports().ok()?;
    for port in ports {
        let serialport::SerialPortType::UsbPort(ref info) = port.port_type else {
            continue;
        };
        let is_ftdi = info.vid == FTDI_VID || info.manufacturer.as_deref() == Some("FTDI");
        let is_maui = info.vid == crate::MAUI_VID && info.pid == crate::MAUI_PID;
        if !is_ftdi && !is_maui {
            continue;
        }
        if let Some(loc) = usb_loc {
            if let Some(cand_loc) = crate::usb::UsbLocation::from_tty_path(&port.port_name) {
                if !loc.matches(&cand_loc) {
                    continue;
                }
            }
        }
        let candidate = Path::new(&port.port_name);
        if let Ok(file) = OpenOptions::new().write(true).read(true).open(candidate) {
            info!("Found re-enumerated serial port on target USB hub location: {}", port.port_name);
            return Some((port.port_name, file));
        }
    }
    None
}

impl Uart {
    pub fn open(path: &String, baud_rate: u32) -> Result<Uart, Error> {
        let baud_rate_speed = match get_uart_speed(baud_rate) {
            Ok(speed) => speed,
            Err(_) => return Err(Error::new(ErrorKind::Other, "Invalid baud rate")),
        };
        info!("Opening UART: {path} with baud rate:{baud_rate} ");

        let usb_loc = crate::usb::UsbLocation::from_tty_path(path);
        let start = std::time::Instant::now();
        let timeout = Duration::from_secs(10);
        let mut target_path = path.clone();
        let mut file_opt = None;

        while start.elapsed() < timeout {
            let p = Path::new(&target_path);
            if let Ok(file) = OpenOptions::new().write(true).read(true).open(p) {
                file_opt = Some(file);
                break;
            }

            // If direct path open fails, scan available ports for FTDI or Maui USB-UART devices
            if let Some((new_path, file)) = try_open_reenumerated_port(usb_loc.as_ref()) {
                target_path = new_path;
                file_opt = Some(file);
                break;
            }

            std::thread::sleep(Duration::from_millis(200));
        }

        let file = match file_opt {
            Some(f) => f,
            None => {
                info!("Serial port {} not found.", path);
                /* Print all available ports */
                Uart::print_all_serial_ports();
                return Err(Error::new(
                    ErrorKind::Other,
                    format!("Serial port {} not found after timeout.", path),
                ));
            }
        };

        trace!("Setting termios settings for the UART");
        let fd = file.as_raw_fd();
        let mut tty = Termios::from_fd(fd)?;
        cfmakeraw(&mut tty);
        tcsetattr(fd, TCSANOW, &tty)?;

        let mut tty = Termios::from_fd(fd)?;

        tty.c_cflag &= !PARENB; // Clear parity bit, disabling parity (most common)
        tty.c_cflag &= !CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
        tty.c_cflag &= !CSIZE; // Clear all bits that set the data size
        tty.c_cflag |= CS8; // 8 bits per byte (most common)
        tty.c_cflag &= !CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

        tty.c_lflag &= !ICANON;
        tty.c_lflag &= !ECHO; // Disable echo
        tty.c_lflag &= !ECHOE; // Disable erasure
        tty.c_lflag &= !ECHONL; // Disable new-line echo
        tty.c_lflag &= !ISIG; // Disable interpretation of INTR, QUIT and SUSP
        tty.c_iflag &= !(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
        tty.c_iflag &= !(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of
                                                                                    // received bytes
        tty.c_oflag &= !OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
        tty.c_oflag &= !ONLCR; // Prevent conversion of newline to carriage return/line feed
        tty.c_cc[VTIME] = 10; // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
        tty.c_cc[VMIN] = 0;

        trace!("Setting baud rate for the UART");
        cfsetispeed(&mut tty, baud_rate_speed)?;
        cfsetospeed(&mut tty, baud_rate_speed)?;

        tcsetattr(fd, TCSANOW, &tty)?;

        let uart = Uart { file };

        Ok(uart)
    }

    /// Scans available USB serial ports for a matching VID and PID, optionally filtered by parent hub serial.
    /// If exactly one port matches, it is returned.
    /// If multiple ports match, an error is returned to prompt explicit user targeting.
    pub fn find_serial_port(vid: u16, pid: u16, hub_serial_filter: Option<&str>) -> Result<String> {
        let start = std::time::Instant::now();
        let timeout = Duration::from_secs(10);
        while start.elapsed() < timeout {
            if let Ok(ports) = serialport::available_ports() {
                let mut matches = Vec::new();
                for p in ports {
                    let serialport::SerialPortType::UsbPort(info) = p.port_type else {
                        continue;
                    };
                    if info.vid != vid || info.pid != pid {
                        continue;
                    }
                    if let Some(target_hub_sn) = hub_serial_filter {
                        if let Some(loc) = crate::usb::UsbLocation::from_tty_path(&p.port_name) {
                            if loc.parent_hub_serial.as_deref() != Some(target_hub_sn) {
                                continue;
                            }
                        }
                    }
                    matches.push(p.port_name);
                }
                if matches.len() == 1 {
                    return Ok(matches.remove(0));
                } else if matches.len() > 1 {
                    let mut loc_strs = Vec::new();
                    for m in &matches {
                        let loc = crate::usb::UsbLocation::from_tty_path(m);
                        let sn = loc.as_ref().and_then(|l| l.parent_hub_serial.as_deref()).unwrap_or("unknown");
                        loc_strs.push(format!("{} (hub serial {})", m, sn));
                    }
                    bail!(
                        "Multiple matching USB serial devices ({:04x}:{:04x}) detected: [{}]. Please specify target device via --hub-serial <SERIAL> or --uart <PORT>.",
                        vid, pid, loc_strs.join(", ")
                    );
                }
            }
            std::thread::sleep(Duration::from_millis(100));
        }
        if let Some(target_hub_sn) = hub_serial_filter {
            bail!("Could not find serial port for USB device {vid:04x}:{pid:04x} on parent hub serial {target_hub_sn}");
        } else {
            bail!("Could not find serial port for USB device {vid:04x}:{pid:04x}");
        }
    }

    pub fn print_all_serial_ports(){
        if let Ok(ports) = serialport::available_ports() {
            if !ports.is_empty() {
                println!("Available ports:");
                for port in ports {
                    let port_info_str = format!("{:?}", port.port_type);
                    let manufacture_reg = Regex::new(r#"manufacturer: Some\("(.*?)"\)"#).unwrap();
                    if let Some(manufacturer) = manufacture_reg.captures(&port_info_str) {
                        /* Extract the manufacturer value from port type */
                        let manufacturer_value = manufacturer.get(1).unwrap().as_str();
                        println!("Name: {}  Manufacturer: {}", port.port_name, manufacturer_value);
                    } else {
                        println!("Name: {}", port.port_name);
                    }
                }
            }
        } else {
            info!("Unable to enumerate serial ports");
        }
    }

    pub fn write_all(&mut self, data: &[u8]) -> Result<()> {
        trace!("Writing {} bytes", data.len());
        self.file.write_all(data)?;
        tcdrain(self.file.as_raw_fd())?;
        Ok(())
    }

    pub fn read_exact(&mut self, buf: &mut [u8]) -> Result<()> {
        trace!("Reading {} bytes from UART", buf.len());
        self.file.read_exact(buf)?;
        Ok(())
    }

    pub fn read_all(&mut self, buf: &mut [u8]) -> Result<()> {
        info!("Reading a line from UART");
        self.file.read(buf)?;
        Ok(())
    }

    pub fn clear_io(&mut self) -> Result<()> {
        // flush any pending commands
        self.write_all("\r".as_bytes())?;
        sleep(Duration::from_secs(1));

        // read all available buffer and discard
        let mut buf = vec![];
        self.file.read_to_end(&mut buf)?;
        Ok(())
    }

    pub fn flush_input(&mut self) -> Result<()> {
        tcflush(self.file.as_raw_fd(), TCIFLUSH)?;
        Ok(())
    }

    pub fn flush(&mut self) -> Result<()> {
        self.file.flush()?;
        Ok(())
    }
}
