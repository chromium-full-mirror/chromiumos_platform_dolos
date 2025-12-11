// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Implements API to send/receive commands/response to Ti BSL (Bootstrap Loader).
/// Spec: https://www.ti.com/lit/ug/slau887/slau887.pdf
use std::iter;
use std::process::exit;
use std::thread::sleep;
use std::time::Duration;

use anyhow::{bail, Result};
use log::{debug, trace, error};

use crate::uart::Uart;


#[allow(dead_code)]
#[derive(Debug)]
enum BslCoreCommandCode {
    Connection = 0x12,
    UnlockBootloader = 0x21,
    GetDeviceInfo = 0x19,
    ChangeBaudRate = 0x52,
    FlashRangeErase = 0x23,
    ProgramData = 0x20,
    StartApplication = 0x40,
}

#[allow(dead_code)]
#[derive(Debug)]
enum BslCoreMessageCode {
    OperationSuccessful = 0,
    BslLockedError = 1,
    BslPasswordError = 2,
    MultipleBslPasswordError = 3,
    UnknownCommand = 4,
    InvalidMemoryRange = 5,
    InvalidCommand = 6,
    FactoryResetDisabled = 7,
    FactoryResetPasswordError = 8,
    ReadOutError = 9,
    InvalidAddressOrLengthAlignment = 0xa,
    InvalidLengthForStandaloneVerification = 0xb,
}

#[allow(dead_code)]
#[derive(PartialEq, Debug)]
enum BslResponseAck {
    Ack = 0,
    ErrorHeaderIncorrect = 0x51,
    ErrorChecksumIncorrect = 0x52,
    ErrorPacketSizeZero = 0x53,
    ErrorPacketSizeTooBig = 0x54,
    ErrorUnknownError = 0x55,
    ErrorUnknownBaudRate = 0x56,
}

#[allow(dead_code)]
#[derive(Debug)]
enum BslCoreResponseCode {
    MemoryReadBack = 0x30,
    GetDeviceInfo = 0x31,
    StandaloneVerification = 0x32,
    Message = 0x3b,
    DetailedError = 0x3a,
}

#[allow(dead_code)]
#[derive(Debug)]
pub enum BslBaudRateOption {
    ACK = 0x00,
    BaudRate4800 = 1,
    BaudRate9600 = 2,
    BaudRate19200 = 3,
    BaudRate38400 = 4,
    BaudRate57600 = 5,
    BaudRate115200 = 6,
    BaudRate1000000 = 7,
    BaudRate2000000 = 8,
    BaudRate3000000 = 9,
    InvalidValue = 0xff,
}

#[allow(dead_code)]
#[derive(Debug)]
pub struct BslGetDeviceInfo {
    command_interpreter_version: u16,
    build_id: u16,
    application_version: u32,
    active_plugin_interface_version: u16,
    bsl_max_buffer_size: u16,
    bsl_buffer_start_address: u32,
    bcr_configuration_id: u32,
    bsl_configuration_id: u32,
}

#[derive(Debug)]
enum BslCoreResponse {
    MemoryReadBack(Vec<u8>),
    GetDeviceInfo(BslGetDeviceInfo),
    StandaloneVerification(u32),
    Message(BslCoreMessageCode),
    DetailedError(u32),
}

pub struct Bsl {
    uart: Uart,
}

impl Bsl {
    /// Open BSL interface over UART
    pub fn open(uart: Uart) -> Result<Bsl> {
        let bsl = Bsl { uart };

        Ok(bsl)
    }

    pub fn connect(&mut self) -> Result<()> {
        debug!("Establishing Connection");
        if let Err(_) = self.issue_command(
            BslCoreCommandCode::Connection as _,
            None,
            None,
            false,
        ) {
            error!("Unable to connect to BSL");
            error!("Check if device is in BSL mode or if another process holds the connection");
            exit(1)
        }
        Ok(())
    }

    /// Return device information reported by BSL
    pub fn get_device_info(&mut self) -> Result<BslGetDeviceInfo> {
        debug!("Issuing GetDeviceInfo");
        let result = self
            .issue_command(
                BslCoreCommandCode::GetDeviceInfo as _,
                None,
                None,
                true,
            )?
            .unwrap();
        match result {
            BslCoreResponse::GetDeviceInfo(v) => Ok(v),
            _ => {
                bail!("Invalid response for get_device_info()")
            }
        }
    }

    /// Sent unlock bootloader command
    pub fn unlock_bootloader(&mut self) -> Result<()> {
        debug!("Issuing UnlockBootloader");
        let default_password = iter::repeat(0xffu8).take(32).collect::<Vec<u8>>();

        self.issue_command(
            BslCoreCommandCode::UnlockBootloader as _,
            None,
            Some(default_password),
            true,
        )?;

        Ok(())
    }

    /// Change the baud rate to new value.
    pub fn change_baud_rate(&mut self, baud_rate: u32) -> Result<()> {
        debug!("ChangeBaudRate");
        let baud_rate_enum = Self::baud_rate_to_enum(baud_rate);
        let result = self.issue_command(
            BslCoreCommandCode::ChangeBaudRate as _,
            None,
            Some(vec![baud_rate_enum as u8]),
            false,
        )?;

        Ok(())
    }

    // helper function to map baud rate to enum variant
    pub fn baud_rate_to_enum(baud_rate: u32) -> BslBaudRateOption {
        match baud_rate {
            4800 => BslBaudRateOption::BaudRate4800,
            9600 => BslBaudRateOption::BaudRate9600,
            19200 => BslBaudRateOption::BaudRate19200,
            38400 => BslBaudRateOption::BaudRate38400,
            57600 => BslBaudRateOption::BaudRate57600,
            115200 => BslBaudRateOption::BaudRate115200,
            1000000 => BslBaudRateOption::BaudRate1000000,
            2000000 => BslBaudRateOption::BaudRate2000000,
            3000000 => BslBaudRateOption::BaudRate3000000,
            _ => BslBaudRateOption::InvalidValue,
        }
    }
    /// Erase the flash
    pub fn erase(&mut self, start_address: u32, end_address: u32) -> Result<()> {
        debug!("Erasing flash from @{start_address:#x} to @{end_address:#x}");
        let erase_end_address: Vec<u8> = end_address.to_le_bytes().to_vec();
        let result = self
            .issue_command(
                BslCoreCommandCode::FlashRangeErase as _,
                Some(start_address),
                Some(erase_end_address),
                true,
            )?
            .unwrap();
        match result {
            BslCoreResponse::Message(v) => match v {
                BslCoreMessageCode::OperationSuccessful => Ok(()),
                _ => {
                    bail!("Flash erase failed : {v:?}")
                }
            },
            _ => {
                bail!("Invalid response for flash erase")
            }
        }
    }

    fn program_chunk(&mut self, address: u32, chunk: &[u8]) -> Result<()> {
        debug!("Writing chunk @{address:#x} with {}bytes", chunk.len());
        let result = self
            .issue_command(
                BslCoreCommandCode::ProgramData as _,
                Some(address),
                Some(chunk.to_vec()),
                true,
            )?
            .unwrap();
        match result {
            BslCoreResponse::Message(v) => match v {
                BslCoreMessageCode::OperationSuccessful => Ok(()),
                _ => {
                    bail!("Program data failed : {v:?}")
                }
            },
            _ => {
                bail!("Invalid response for program data")
            }
        }
    }

    const BSL_PACKET_MAX_DATA_SIZE: u32 = 2048;
    /// Program the given data into flash
    pub fn program(
        &mut self,
        address: u32,
        data: Vec<u8>,
        progress_cb: &dyn Fn(u32, u32),
    ) -> Result<()> {
        debug!(
            "Programming @{address:#x} image with size {} bytes",
            data.len()
        );
        let mut cur_address = address;
        let mut written = 0u32;
        for chunk in data
            .chunks(Self::BSL_PACKET_MAX_DATA_SIZE as usize)
            .collect::<Vec<_>>()
        {
            self.program_chunk(cur_address, chunk)?;
            cur_address += Self::BSL_PACKET_MAX_DATA_SIZE;
            written += chunk.len() as u32;
            progress_cb(data.len() as _, written);
        }
        Ok(())
    }

    /// Exit BSL and start firmware
    pub fn start_application(&mut self) -> Result<()> {
        debug!("Starting firmware");
        let _ = self.issue_command(
            BslCoreCommandCode::StartApplication as _,
            None,
            None,
            false,
        )?;
        Ok(())
    }

    /// Calculate CRC32 for given data bytes.
    fn calculate_crc(data: &[u8]) -> u32 {
        const CRC: crc::Crc<u32> = crc::Crc::<u32>::new(&crc::CRC_32_JAMCRC);
        CRC.checksum(data)
    }

    /// Construct byte array contains cmd packet to send to the BSL
    fn construct_cmd_packet(cmd: u8, address: Option<u32>, data: Option<Vec<u8>>) -> Vec<u8> {
        let mut buf: Vec<u8> = vec![0x80];
        let mut len: u16 = 1;
        if let Some(ref d) = data {
            len += d.len() as u16;
        }
        if address.is_some() {
            len += 4;
        }
        let mut len_bytes = len.to_le_bytes().to_vec();
        buf.append(&mut len_bytes);
        buf.push(cmd);
        if let Some(a) = address {
            let mut b = a.to_le_bytes().to_vec();
            buf.append(&mut b);
        }
        if let Some(mut d) = data {
            buf.append(&mut d);
        }
        // calculate CRC after header
        let crc32 = Self::calculate_crc(&buf[3..]);
        let mut b = crc32.to_le_bytes().to_vec();
        buf.append(&mut b);

        buf
    }

    /// Parse byte array response from BSL into structure.
    fn parse_response(response_code: BslCoreResponseCode, data: &[u8]) -> BslCoreResponse {
        match response_code {
            BslCoreResponseCode::MemoryReadBack => BslCoreResponse::MemoryReadBack(data.to_vec()),
            BslCoreResponseCode::GetDeviceInfo => {
                let v: BslGetDeviceInfo = unsafe { std::ptr::read(data.as_ptr() as *const _) };
                BslCoreResponse::GetDeviceInfo(v)
            }
            BslCoreResponseCode::StandaloneVerification => {
                BslCoreResponse::StandaloneVerification(0)
            }
            BslCoreResponseCode::Message => {
                BslCoreResponse::Message(unsafe { std::mem::transmute(data[0]) })
            }
            BslCoreResponseCode::DetailedError => BslCoreResponse::DetailedError(0),
        }
    }

        /// Receive BSLCoreResponse packet for previous command packet.
        ///
        /// If `first_byte` is provided, it is used as the first byte of the header.
        /// This is useful when the header start byte (0x08) was read during the ACK check.
        fn receive_bsl_core_response(&mut self, first_byte: Option<u8>) -> Result<BslCoreResponse> {
            trace!("Waiting for BSL core response");
            let mut header = vec![0u8; 4];

            if let Some(b) = first_byte {
                header[0] = b;
                self.uart.read_exact(&mut header[1..])?;
            } else {
                self.uart.read_exact(&mut header)?;
            }

            if header[0] != 0x8 {
                bail!("Invalid response message header {:?}", header);
            }
            let len: usize = ((header[2] as usize) << 8 | header[1] as usize) + 3;
            let mut data = vec![0u8; len];
            self.uart.read_exact(&mut data)?;
            trace!("Data from BSL: {:x?}", data);

            let response_code: BslCoreResponseCode = unsafe { std::mem::transmute(header[3]) };
            let result = Self::parse_response(response_code, &data);
            Ok(result)
        }

        /// Receive BSL ack code for previous command packet.
        ///
        /// Returns `AckResult` to distinguish between a standard ACK,
        /// a response header start (indicating the device skipped ACK or we missed it),
        /// or an unknown/error byte.
        fn receive_bsl_ack(&mut self) -> Result<AckResult> {
            debug!("Waiting for BSL ack");
            let mut ack_buf = vec![0u8; 1];
            self.uart.read_exact(&mut ack_buf)?;
            let byte = ack_buf[0];
            trace!("Read Ack response of : {byte:#02x}");
            match byte {
                0x00 => Ok(AckResult::Ack),
                0x08 => Ok(AckResult::Header(byte)),
                _ => Ok(AckResult::Unknown(byte)),
            }
        }

        /// Send command to BSL and wait for response for the command.
        ///
        /// Implements a retry mechanism (3 attempts) and flushes the input buffer
        /// before sending to ensure reliability. Handles cases where the device
        /// responds immediately with data instead of an ACK.
        fn issue_command(
            &mut self,
            cmd: u8,
            address: Option<u32>,
            data: Option<Vec<u8>>,
            expect_bsl_core_response: bool,
        ) -> Result<Option<BslCoreResponse>> {
            let packet = Self::construct_cmd_packet(cmd, address, data);
            let mut retries = 5;

            loop {
                trace!("Issuing command {packet:x?}");
                // Flush input buffer to remove any stale data or garbage
                if let Err(e) = self.uart.flush_input() {
                    debug!("Failed to flush input: {e}");
                }

                if let Err(e) = self.uart.write_all(&packet) {
                    debug!("Failed to write packet: {e}");
                } else {
                    match self.receive_bsl_ack() {
                        Ok(AckResult::Ack) => {
                            if !expect_bsl_core_response {
                                return Ok(None);
                            }
                            match self.receive_bsl_core_response(None) {
                                Ok(result) => {
                                    trace!("Received response = {result:x?}");
                                    return Ok(Some(result));
                                }
                                Err(e) => debug!("Failed to receive core response: {e}"),
                            }
                        }
                        Ok(AckResult::Header(b)) => {
                             // Device sent response immediately (missed ACK or no ACK)
                             if !expect_bsl_core_response {
                                // Unexpected response
                                debug!("Received unexpected response header: {b:#02x}");
                             } else {
                                match self.receive_bsl_core_response(Some(b)) {
                                    Ok(result) => {
                                        trace!("Received response (direct) = {result:x?}");
                                        return Ok(Some(result));
                                    }
                                    Err(e) => debug!("Failed to receive core response (direct): {e}"),
                                }
                             }
                        }
                        Ok(AckResult::Unknown(b)) => {
                             debug!("Received unknown byte awaiting ACK: {b:#02x}");
                             match b {
                                0x51 => debug!("BSL Error: Header Incorrect"),
                                0x52 => debug!("BSL Error: Checksum Incorrect"),
                                0x53 => debug!("BSL Error: Packet Size Zero"),
                                0x54 => debug!("BSL Error: Packet Size Too Big"),
                                0x55 => debug!("BSL Error: Unknown Error"),
                                0x56 => debug!("BSL Error: Unknown Baud Rate"),
                                _ => {}
                             }
                        }
                        Err(e) => debug!("Failed to receive ACK: {e}"),
                    }
                }

                retries -= 1;
                if retries == 0 {
                    bail!("Command failed after retries");
                }
                debug!("Retrying command...");
                sleep(Duration::from_millis(100));
            }
        }
    }

    /// Result of waiting for an ACK from the BSL device.
    enum AckResult {
        /// Standard ACK received (0x00).
        Ack,
        /// Response header start byte received (0x08), implying ACK was skipped or lost.
        Header(u8),
        /// Unknown or error byte received.
        Unknown(u8),
    }
