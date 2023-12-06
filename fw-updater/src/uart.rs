// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::os::fd::AsRawFd;
use std::path::Path;

use anyhow::Result;
use log::{info, trace};
use termios::os::linux::CRTSCTS;
use termios::*;

pub struct Uart {
    file: File,
}

impl Uart {
    pub fn open(path: &String, baud_rate: speed_t) -> Result<Uart> {
        info!("Opening UART: {path} with baud rate:{baud_rate} ",);
        let p = Path::new(path);
        let file = OpenOptions::new().write(true).read(true).open(p)?;

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
        tty.c_cc[VTIME] = 100; // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
        tty.c_cc[VMIN] = 0;

        trace!("Setting baud rate for the UART");
        cfsetispeed(&mut tty, B9600)?;
        cfsetospeed(&mut tty, B9600)?;

        tcsetattr(fd, TCSANOW, &tty)?;

        let uart = Uart { file };

        Ok(uart)
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

    pub fn flush(&mut self) -> Result<()> {
        self.file.flush()?;
        Ok(())
    }
}
