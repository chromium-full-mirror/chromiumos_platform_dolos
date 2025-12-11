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

impl Uart {
    pub fn open(path: &String, baud_rate: u32) -> Result<Uart,Error> {
        let baud_rate = match get_uart_speed(baud_rate) {
            Ok(speed) => speed,
            Err(_) => return Err(Error::new(ErrorKind::Other, "Invalid baud rate")),
        };
        info!("Opening UART: {path} with baud rate:{baud_rate} ");
        let p = Path::new(path);
        let file = match OpenOptions::new().write(true).read(true).open(p) {
            Ok(file) => file,
            Err(_) => {
                info!("Serial port {} not found.", path);
                /* Print all available ports */
                Uart::print_all_serial_ports();
                return Err(Error::new(ErrorKind::Other, format!("Serial port {} not found.", path)))
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
        cfsetispeed(&mut tty, baud_rate)?;
        cfsetospeed(&mut tty, baud_rate)?;

        tcsetattr(fd, TCSANOW, &tty)?;

        let uart = Uart { file };

        Ok(uart)
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

    pub fn flush_input(&self) -> Result<()> {
        tcflush(self.file.as_raw_fd(), TCIFLUSH)?;
        Ok(())
    }

    pub fn flush(&mut self) -> Result<()> {
        self.file.flush()?;
        Ok(())
    }
}
