// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Smart Battery SMBus config definition and methods to read the config through i2c bridge.

extern crate anyhow;
extern crate i2cdev;

use std::fmt;
use std::fmt::Formatter;
use std::fs::File;
use std::io::{Read, Write};

use anyhow::{bail, Result};
use log::debug;
use serde::{Deserialize, Serialize};

use crate::smart_battery::SBRegisterValue::{BlockData, Reserved, Word};
use i2cdev::core::*;
use i2cdev::linux::LinuxI2CDevice;
use zerocopy::AsBytes;
use zerocopy::FromBytes;

use sb_config::eeprom_layout;
use sb_config::eeprom_layout::eeprom_data;

/// Smart Battery Register value types
#[derive(Default, Debug, Serialize, Deserialize)]
pub enum SBRegisterValue {
    /// SB spec block data - can be 1 to 32 bytes.
    BlockData(Vec<u8>),
    /// SB spec word data.
    Word(u16),
    #[default]
    /// Unspecified in the SB spec.
    Reserved,
}

/// SB Register
#[derive(Default, Debug, Serialize, Deserialize)]
pub struct SBRegister {
    /// Name of the register - Just for debugging not sent to FW.
    name: String,
    /// Value read from the register.
    pub(crate) value: SBRegisterValue,
}

/// All the registers in the spec.
#[derive(Default, Debug, Serialize, Deserialize)]
pub struct SBConfig {
    pub(crate) registers: Vec<SBRegister>,
}

impl fmt::Display for SBConfig {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        writeln!(f)?;
        for (addr, reg) in self.registers.iter().enumerate() {
            write!(f, " {addr:#5x} {:30} ", reg.name)?;
            match &reg.value {
                BlockData(v) => {
                    let s = std::str::from_utf8(v).unwrap_or("");
                    let s = s.replace(|c: char| !c.is_alphanumeric(), "");
                    write!(f, "{s:<32} {v:x?}")?;
                }
                Word(v) => {
                    write!(f, "{:#x}", *v)?;
                }
                Reserved => {}
            }
            writeln!(f)?;
        }
        Ok(())
    }
}

impl SBConfig {
    pub fn new() -> SBConfig {
        SBConfig {
            registers: vec![
                sb_u16_register("ManufacturerAccess"),
                sb_u16_register("RemainingCapacityAlarm"),
                sb_u16_register("RemainingTimeAlarm"),
                sb_u16_register("BatteryMode"),
                sb_u16_register("AtRate"),
                sb_u16_register("AtRateTimeToFull"),
                sb_u16_register("AtRateTimeToEmpty"),
                sb_u16_register("AtRateOK"),
                sb_u16_register("Temperature"),
                sb_u16_register("Voltage"),
                sb_u16_register("Current"),
                sb_u16_register("AverageCurrent"),
                sb_u16_register("MaxError"),
                sb_u16_register("RelativeStateOfCharge"),
                sb_u16_register("AbsoluteStateOfCharge"),
                sb_u16_register("RemainingCapacity"),
                sb_u16_register("FullChargeCapacity"),
                sb_u16_register("RunTimeToEmpty"),
                sb_u16_register("AverageTimeToEmpty"),
                sb_u16_register("AverageTimeToFull"),
                sb_u16_register("ChargingCurrent"),
                sb_u16_register("ChargingVoltage"),
                sb_u16_register("BatteryStatus"),
                sb_u16_register("CycleCount"),
                sb_u16_register("DesignCapacity"),
                sb_u16_register("DesignVoltage"),
                sb_u16_register("SpecificationInfo"),
                sb_u16_register("ManufactureDate"),
                sb_u16_register("SerialNumber"),
                sb_reserved_register(), // 0x1d
                sb_reserved_register(), // 0x1e
                sb_reserved_register(), // 0x1f
                sb_string_register("ManufacturerName"),
                sb_string_register("DeviceName"),
                sb_string_register("DeviceChemistry"),
                sb_string_register("ManufacturerData"),
                sb_reserved_register(), // 0x24
                sb_reserved_register(), // 0x25
                sb_reserved_register(), // 0x26
                sb_reserved_register(), // 0x27
                sb_reserved_register(), // 0x28
                sb_reserved_register(), // 0x29
                sb_reserved_register(), // 0x2a
                sb_reserved_register(), // 0x2b
                sb_reserved_register(), // 0x2c
                sb_reserved_register(), // 0x2d
                sb_reserved_register(), // 0x2e
                sb_u16_register("OptionalMfgFunction5"),
                sb_reserved_register(), // 0x30
                sb_reserved_register(), // 0x31
                sb_reserved_register(), // 0x32
                sb_reserved_register(), // 0x33
                sb_reserved_register(), // 0x34
                sb_reserved_register(), // 0x35
                sb_reserved_register(), // 0x36
                sb_reserved_register(), // 0x37
                sb_reserved_register(), // 0x38
                sb_reserved_register(), // 0x39
                sb_reserved_register(), // 0x3a
                sb_reserved_register(), // 0x3b
                sb_u16_register("OptionalMfgFunction4"),
                sb_u16_register("OptionalMfgFunction3"),
                sb_u16_register("OptionalMfgFunction2"),
                sb_u16_register("OptionalMfgFunction1"),
                sb_u16_register("AlarmWarning"),
            ],
        }
    }

    /// Create a new SBConfig from eeprom_data
    pub fn from_eeprom_data(eeprom_data: eeprom_layout::eeprom_data) -> SBConfig {
        let mut sb_config: SBConfig = SBConfig::new();
        let mut block_reg_index = 0;
        let mut word_reg_index = 0;
        for (i, sb_reg) in sb_config.registers.iter_mut().enumerate() {
            match &mut sb_reg.value {
                BlockData(v) => {
                    if i == 0x2f {
                        continue;
                    }
                    assert!(i == 0x20 || i == 0x21 || i == 0x22 || i == 0x23 || i == 0x44);
                    for j in 0..eeprom_data.block_registers[block_reg_index].length as usize {
                        v.push(eeprom_data.block_registers[block_reg_index].data[j]);
                    }
                    block_reg_index += 1;
                }
                Word(v) => {
                    *v = eeprom_data.word_register[word_reg_index];
                    word_reg_index += 1;
                }
                Reserved => {}
            }
        }
        sb_config
    }
}

/// Helper to create word register.
fn sb_u16_register(name: &str) -> SBRegister {
    SBRegister {
        name: name.to_string(),
        value: Word(0),
    }
}

/// Helper to create empty block register.
fn sb_string_register(name: &str) -> SBRegister {
    SBRegister {
        name: name.to_string(),
        value: BlockData(vec![]),
    }
}

fn sb_reserved_register() -> SBRegister {
    SBRegister {
        name: "reserved".to_string(),
        value: Reserved,
    }
}

/// Read config from SB
pub fn read_config(usb_i2c_bridge_address: u16) -> Result<SBConfig> {
    let smbus_address: u16 = 0xb;
    let mut result: SBConfig = SBConfig::new();

    let path = format!("/dev/i2c-{usb_i2c_bridge_address}");
    let mut dev = match LinuxI2CDevice::new(path.clone(), smbus_address) {
        Ok(v) => v,
        Err(e) => {
            let msg = format!("Failed to open USB i2c bridge {path}: {e}");
            bail!(msg);
        }
    };
    for (register_address, mut reg) in result.registers.iter_mut().enumerate() {
        match reg.value {
            BlockData(_) => {
                debug!("Reading block data at {register_address:#x}");
                let value = dev.smbus_read_block_data(register_address as u8)?;
                debug!("Read byte {}", value.len());
                reg.value = BlockData(value);
            }
            Word(_) => {
                debug!("Reading word at {register_address:#x}");
                let value = dev.smbus_read_word_data(register_address as u8)?;
                reg.value = Word(value);
            }
            Reserved => {}
        }
    }

    Ok(result)
}

/// Calculate CRC32 for given data bytes.
fn calculate_crc(data: &[u8]) -> u32 {
    const CRC: crc::Crc<u32> = crc::Crc::<u32>::new(&crc::CRC_32_JAMCRC);
    CRC.checksum(data)
}

/// Returns the starting offset of the SB registers in the EEPROM layout.
fn offset_of_sb_register() -> usize {
    12
}

/// Save the config into a binary file.
pub fn save_config(file_name: &String, sb_config: &SBConfig) -> Result<()> {
    let mut f = File::create(file_name)?;
    let mut r: eeprom_layout::eeprom_data = eeprom_data {
        version: 1,
        polarity: 1,
        manufactured_year: 24,
        manufactured_month: 1,
        serial_number: 1234,
        reserved: Default::default(),
        crc: 0,
        block_registers: Default::default(),
        word_register: [0; 36],
    };
    let mut block_reg_index = 0;
    let mut word_reg_index = 0;
    for (i, sb_reg) in sb_config.registers.iter().enumerate() {
        match &sb_reg.value {
            BlockData(v) => {
                if i == 0x2f {
                    continue;
                }
                assert!(i == 0x20 || i == 0x21 || i == 0x22 || i == 0x23 || i == 0x44);
                for (j, value) in v.iter().enumerate() {
                    r.block_registers[block_reg_index].data[j] = *value;
                }
                r.block_registers[block_reg_index].length = v.len() as u8;
                block_reg_index += 1;
            }
            Word(v) => {
                r.word_register[word_reg_index] = *v;
                word_reg_index += 1;
            }
            Reserved => {}
        }
    }

    r.crc = calculate_crc(&r.as_bytes()[offset_of_sb_register()..]);
    f.write_all(r.as_bytes())?;

    Ok(())
}

/// Open config stored in a file.
pub fn load_config(file_name: &String) -> Result<(eeprom_layout::eeprom_data, SBConfig)> {
    let mut f = File::open(file_name)?;
    let mut buf: [u8; 256] = [0; 256];
    f.read_exact(&mut buf)?;
    let data: eeprom_layout::eeprom_data = FromBytes::read_from(buf.as_slice()).unwrap();
    let sb_config = SBConfig::from_eeprom_data(data);

    Ok((data, sb_config))
}
