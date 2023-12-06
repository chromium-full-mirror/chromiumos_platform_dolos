// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Implements API to send/receive commands/response to Ti BSL (Bootstrap Loader).
/// Spec: https://www.ti.com/lit/ug/slau887/slau887.pdf
use std::iter;
use std::thread::sleep;
use std::time::Duration;

use anyhow::{bail, Result};
use log::{debug, trace};

use crate::uart::Uart;

const BSL_DEFAULT_CMD_WAIT_MS: u64 = 500;
const BSL_FLASH_CMD_WAIT_MS: u64 = 100;

#[allow(dead_code)]
#[derive(Debug)]
enum BslCoreCommandCode {
    Connection = 0x12,
    UnlockBootloader = 0x21,
    GetDeviceInfo = 0x19,
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
        let _ = self.issue_command(
            BslCoreCommandCode::Connection as _,
            None,
            None,
            false,
            BSL_DEFAULT_CMD_WAIT_MS,
        )?;
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
                BSL_DEFAULT_CMD_WAIT_MS,
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
            BSL_DEFAULT_CMD_WAIT_MS,
        )?;

        Ok(())
    }

    /// Erase the flash
    pub fn erase(&mut self) -> Result<()> {
        debug!("Erasing flash");
        let program_start_address: u32 = 0;
        let program_end_address: Vec<u8> = 0xfc00u32.to_le_bytes().to_vec();
        let result = self
            .issue_command(
                BslCoreCommandCode::FlashRangeErase as _,
                Some(program_start_address),
                Some(program_end_address),
                true,
                BSL_DEFAULT_CMD_WAIT_MS,
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
                BSL_FLASH_CMD_WAIT_MS,
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
            BSL_DEFAULT_CMD_WAIT_MS,
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

    /// Receive BSL ack code for previous command packet.
    fn receive_bsl_ack(&mut self) -> Result<BslResponseAck> {
        debug!("Waiting for BSL ack");
        let mut ack_buf = vec![0u8; 1];
        self.uart.read_exact(&mut ack_buf)?;
        trace!("Read Ack response of : {ack_buf:?}");
        let code = unsafe { std::mem::transmute(ack_buf[0]) };
        Ok(code)
    }

    /// Receive BSLCoreResponse packet for previous command packet.
    fn receive_bsl_core_response(&mut self) -> Result<BslCoreResponse> {
        trace!("Waiting for BSL core response");
        let mut header = vec![0u8; 4];
        self.uart.read_exact(&mut header)?;
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

    /// Send command to BSL and wait for response for the command.
    fn issue_command(
        &mut self,
        cmd: u8,
        address: Option<u32>,
        data: Option<Vec<u8>>,
        expect_bsl_core_response: bool,
        wait_before_receive_ms: u64,
    ) -> Result<Option<BslCoreResponse>> {
        let packet = Self::construct_cmd_packet(cmd, address, data);
        trace!("Issuing command {packet:x?}");
        self.uart.write_all(&packet)?;

        sleep(Duration::from_millis(wait_before_receive_ms));

        let ack = self.receive_bsl_ack()?;
        if ack != BslResponseAck::Ack {
            bail!("Command failed with {ack:?}");
        }

        // if no core response is expected, then return now
        if !expect_bsl_core_response {
            return Ok(None);
        }

        let result = self.receive_bsl_core_response()?;
        trace!("Received response = {result:x?}");
        Ok(Some(result))
    }
}
