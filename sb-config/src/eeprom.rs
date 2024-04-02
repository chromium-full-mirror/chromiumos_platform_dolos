// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Smart Battery SMBus config definition and methods to read the config through i2c bridge.

extern crate anyhow;
extern crate i2cdev;

use std::collections::HashMap;
use std::fmt;
use std::fmt::Formatter;
use std::fs::File;
use std::io::{Read, Write};

use anyhow::{bail, Result};
use log::debug;
use memoffset::offset_of;
use serde::{Deserialize, Serialize};

use crate::eeprom::SBRegisterValue::{BlockRegister, Reserved, WordRegister};
use i2cdev::core::*;
use i2cdev::linux::LinuxI2CDevice;
use zerocopy::AsBytes;
use zerocopy::FromBytes;

use sb_config::eeprom_layout;
use sb_config::eeprom_layout::*;

/// Smart Battery Register value types
#[derive(Default, Debug, Serialize, Deserialize)]
pub enum SBRegisterValue {
    /// SB spec block data - can be 1 to 32 bytes.
    BlockRegister(Vec<u8>),
    /// SB spec word data.
    WordRegister(u16),
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
pub struct SBRegisterSet {
    pub(crate) registers: HashMap<u8, SBRegister>,
}

impl fmt::Display for SBRegisterSet {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        writeln!(f)?;
        let mut addresses: Vec<u8> = self.registers.keys().cloned().collect();
        addresses.sort();
        for addr in addresses {
            let reg = self.registers.get(&addr).unwrap();
            write!(f, " {addr:#5x} {:30} ", reg.name)?;
            match &reg.value {
                BlockRegister(v) => {
                    let s = std::str::from_utf8(v).unwrap_or("");
                    let s = s.replace(|c: char| !c.is_alphanumeric(), "");
                    write!(f, "{s:<32} {v:x?}")?;
                }
                WordRegister(v) => {
                    write!(f, "{:#x}", *v)?;
                }
                Reserved => {}
            }
            writeln!(f)?;
        }
        Ok(())
    }
}

fn eeprom_layout_get_block_register_offset(address: u8) -> usize {
    let m = HashMap::from([
        (SB_MANUFACTURER_NAME as u8, 0),
        (SB_DEVICE_NAME as u8, 1),
        (SB_DEVICE_CHEMISTRY as u8, 2),
        (SB_MANUFACTURER_DATA as u8, 3),
        (SB_ALT_MANUFACTURER_ACCESS as u8, 4),
        (SB_MANUFACTURE_INFO as u8, 5),
    ]);
    let result: u8 = *m.get(&address).unwrap();
    result as usize
}

fn eeprom_layout_get_word_register_offset(address: u8) -> usize {
    address as _
}

impl SBRegisterSet {
    pub fn new(not_present_registers: &Vec<u8>) -> SBRegisterSet {
        let mut result = SBRegisterSet {
            registers: HashMap::from([
                (SB_MANUFACTURER_ACCESS as u8, sb_u16("ManufacturerAccess")),
                (SB_REMAINING_CAPACITY_ALARM as u8, sb_u16("RemCapAlarm")),
                (SB_REMAINING_TIME_ALARM as u8, sb_u16("RemTimeAlarm")),
                (SB_BATTERY_MODE as u8, sb_u16("BatteryMode")),
                (SB_AT_RATE as u8, sb_u16("AtRate")),
                (SB_AT_RATE_TIME_TO_FULL as u8, sb_u16("AtRateTimeToFull")),
                (SB_AT_RATE_TIME_TO_EMPTY as u8, sb_u16("AtRateTimeToEmpty")),
                (SB_AT_RATE_OK as u8, sb_u16("AtRateOK")),
                (SB_TEMPERATURE as u8, sb_u16("Temperature")),
                (SB_VOLTAGE as u8, sb_u16("Voltage")),
                (SB_CURRENT as u8, sb_u16("Current")),
                (SB_AVERAGE_CURRENT as u8, sb_u16("AverageCurrent")),
                (SB_MAX_ERROR as u8, sb_u16("MaxError")),
                (SB_RELATIVE_STATE_OF_CHARGE as u8, sb_u16("RelChargeState")),
                (SB_ABSOLUTE_STATE_OF_CHARGE as u8, sb_u16("AbsChargeState")),
                (SB_REMAINING_CAPACITY as u8, sb_u16("RemainingCapacity")),
                (SB_FULL_CHARGE_CAPACITY as u8, sb_u16("FullChargeCapacity")),
                (SB_RUN_TIME_TO_EMPTY as u8, sb_u16("RunTimeToEmpty")),
                (SB_AVERAGE_TIME_TO_EMPTY as u8, sb_u16("AverageTimeToEmpty")),
                (SB_AVERAGE_TIME_TO_FULL as u8, sb_u16("AverageTimeToFull")),
                (SB_CHARGING_CURRENT as u8, sb_u16("ChargingCurrent")),
                (SB_CHARGING_VOLTAGE as u8, sb_u16("ChargingVoltage")),
                (SB_BATTERY_STATUS as u8, sb_u16("BatteryStatus")),
                (SB_CYCLE_COUNT as u8, sb_u16("CycleCount")),
                (SB_DESIGN_CAPACITY as u8, sb_u16("DesignCapacity")),
                (SB_DESIGN_VOLTAGE as u8, sb_u16("DesignVoltage")),
                (SB_SPECIFICATION_INFO as u8, sb_u16("SpecificationInfo")),
                (SB_MANUFACTURE_DATE as u8, sb_u16("ManufactureDate")),
                (SB_SERIAL_NUMBER as u8, sb_u16("SerialNumber")),
                (SB_MANUFACTURER_NAME as u8, sb_string("ManufacturerName")),
                (SB_DEVICE_NAME as u8, sb_string("DeviceName")),
                (SB_DEVICE_CHEMISTRY as u8, sb_string("DeviceChemistry")),
                (SB_MANUFACTURER_DATA as u8, sb_string("ManufacturerData")),
                (SB_OPTIONAL_MFG_FUNC1 as u8, sb_u16("OptionalMfgFunction4")),
                (SB_OPTIONAL_MFG_FUNC2 as u8, sb_u16("OptionalMfgFunction3")),
                (SB_OPTIONAL_MFG_FUNC3 as u8, sb_u16("OptionalMfgFunction2")),
                (SB_OPTIONAL_MFG_FUNC4 as u8, sb_u16("OptionalMfgFunction1")),
                (SB_PACK_STATUS as u8, sb_u16("PackStatus")),
                (SB_ALT_MANUFACTURER_ACCESS as u8, sb_string("AltMfrAccess")),
                (SB_MANUFACTURE_INFO as u8, sb_string("ManufacturerInfo")),
            ]),
        };

        for address in not_present_registers {
            result.registers.remove(address);
        }

        result
    }

    /// Create a new SBRegisterSet from eeprom_data
    pub fn from_eeprom_data(eeprom_data: eeprom_layout::eeprom_data) -> SBRegisterSet {
        let mut sb_config: SBRegisterSet = SBRegisterSet::new(&vec![]);
        for (i, sb_reg) in sb_config.registers.iter_mut() {
            let i = *i;
            match &mut sb_reg.value {
                BlockRegister(v) => {
                    let reg_offset = eeprom_layout_get_block_register_offset(i);
                    for j in 0..eeprom_data.block_registers[reg_offset].length as usize {
                        v.push(eeprom_data.block_registers[reg_offset].data[j]);
                    }
                }
                WordRegister(v) => {
                    let reg_offset = eeprom_layout_get_word_register_offset(i);
                    if eeprom_data.word_registers[reg_offset].present != 0 {
                        *v = eeprom_data.word_registers[reg_offset].data;
                    }
                }
                Reserved => {}
            }
        }
        sb_config
    }
}

/// Calculate CRC32 for given data bytes.
fn calculate_crc(data: &[u8]) -> u32 {
    const CRC: crc::Crc<u32> = crc::Crc::<u32>::new(&crc::CRC_32_JAMCRC);
    CRC.checksum(data)
}

/// Helper to create word register.
fn sb_u16(name: &str) -> SBRegister {
    SBRegister {
        name: name.to_string(),
        value: WordRegister(0),
    }
}

/// Helper to create empty block register.
fn sb_string(name: &str) -> SBRegister {
    SBRegister {
        name: name.to_string(),
        value: BlockRegister(vec![]),
    }
}

/// Read Smart Battery Registers over I2c and create SBRegisterSet
pub fn read_sb_registers(
    usb_i2c_bridge_address: u16,
    dont_read_extension_registers: bool,
) -> Result<SBRegisterSet> {
    let smbus_address: u16 = 0xb;
    let not_present_registers: Vec<u8> = if dont_read_extension_registers {
        vec![
            SB_PACK_STATUS as u8,
            SB_ALT_MANUFACTURER_ACCESS as u8,
            SB_MANUFACTURE_INFO as u8,
        ]
    } else {
        vec![]
    };

    let mut result: SBRegisterSet = SBRegisterSet::new(&not_present_registers);
    let path = format!("/dev/i2c-{usb_i2c_bridge_address}");
    let mut dev = match LinuxI2CDevice::new(path.clone(), smbus_address) {
        Ok(v) => v,
        Err(e) => {
            let msg = format!("Failed to open USB i2c bridge {path}: {e}");
            bail!(msg);
        }
    };
    for (address, reg) in result.registers.iter_mut() {
        let address = *address;
        match reg.value {
            BlockRegister(_) => {
                debug!("Reading block data at {address:#x}");
                let value = dev.smbus_read_block_data(address)?;
                debug!("Read byte {}", value.len());
                reg.value = BlockRegister(value);
            }
            WordRegister(_) => {
                debug!("Reading word at {address:#x}");
                let value = dev.smbus_read_word_data(address)?;
                reg.value = WordRegister(value);
            }
            Reserved => {}
        }
    }

    Ok(result)
}

/// Save the SBRegisterSet into a file.
pub fn write_eeprom_layout_to_file(
    file_name: &String,
    polarity: u8,
    sb_config: &SBRegisterSet,
) -> Result<()> {
    let mut f = File::create(file_name)?;
    let mut r: eeprom_layout::eeprom_data = eeprom_data {
        version: 1,
        polarity,
        manufactured_year: 24,
        manufactured_week: 1,
        serial_number: 1234,
        crc: 0,
        block_registers: Default::default(),
        word_registers: [Default::default(); eeprom_layout::SB_TOTAL_WORD_REGISTERS as usize],
        reserved_1: std::array::from_fn(|_| Default::default()),
        reserved_2: std::array::from_fn(|_| Default::default()),
        reserved_3: std::array::from_fn(|_| Default::default()),
    };
    for (i, sb_reg) in sb_config.registers.iter() {
        match &sb_reg.value {
            BlockRegister(v) => {
                let block_reg_index = eeprom_layout_get_block_register_offset(*i);
                for (j, value) in v.iter().enumerate() {
                    r.block_registers[block_reg_index].data[j] = *value;
                }
                r.block_registers[block_reg_index].length = v.len() as u8;
            }
            WordRegister(v) => {
                r.word_registers[*i as usize] = eeprom_layout::sb_word_register {
                    present: 1,
                    data: *v,
                };
            }
            Reserved => {}
        }
    }

    let crc_start_offset = offset_of!(eeprom_layout::eeprom_data, reserved_1);
    r.crc = calculate_crc(&r.as_bytes()[crc_start_offset..]);
    f.write_all(r.as_bytes())?;

    Ok(())
}

/// Read eeprom layout stroed in the given file and construct SBRegisterSet from it.
pub fn read_sb_registers_set_from_file(
    file_name: &String,
) -> Result<(eeprom_layout::eeprom_data, SBRegisterSet)> {
    let mut f = File::open(file_name)?;
    let mut buf: [u8; 1024] = [0; 1024];
    f.read_exact(&mut buf)?;
    let data: eeprom_layout::eeprom_data = FromBytes::read_from(buf.as_slice()).unwrap();
    let sb_config = SBRegisterSet::from_eeprom_data(data);

    Ok((data, sb_config))
}
