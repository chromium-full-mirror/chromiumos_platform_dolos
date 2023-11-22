/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SMART_BATTERY_H_
#define SMART_BATTERY_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SB_REG_DATA_MAX_SIZE 32

enum sb_register_address {
        SB_REG_MANUFACTURER_ACCESS = 0x0,
        SB_REG_REMAINING_CAPACITY_ALARM = 0x01,
        SB_REG_REMAINING_TIME_ALARM = 0x02,
        SB_REG_BATTERY_MODE = 0x03,
        SB_REG_AT_RATE = 0x04,
        SB_REG_AT_RATE_TIME_TO_FULL = 0x05,
        SB_REG_AT_RATE_TIME_TO_EMPTY = 0x06,
        SB_REG_AT_RATE_OK = 0x07,
        SB_REG_TEMPERATURE = 0x08,
        SB_REG_VOLTAGE = 0x09,
        SB_REG_CURRENT = 0x0a,
        SB_REG_AVERAGE_CURRENT = 0x0b,
        SB_REG_MAX_ERROR = 0x0c,
        SB_REG_RELATIVE_STATE_OF_CHARGE = 0x0d,
        SB_REG_ABSOLUTE_STATE_OF_CHARGE = 0x0e,
        SB_REG_REMAINING_CAPACITY = 0x0f,
        SB_REG_FULL_CHARGE_CAPACITY = 0x10,
        SB_REG_RUN_TIME_TO_EMPTY = 0x11,
        SB_REG_AVERAGE_TIME_TO_EMPTY = 0x12,
        SB_REG_AVERAGE_TIME_TO_FULL = 0x13,
        SB_REG_CHARGING_CURRENT = 0x14,
        SB_REG_CHARGING_VOLTAGE = 0x15,
        SB_REG_BATTERY_STATUS = 0x16,
        SB_REG_CYCLE_COUNT = 0x17,
        SB_REG_DESIGN_CAPACITY = 0x18,
        SB_REG_DESIGN_VOLTAGE = 0x19,
        SB_REG_SPECIFICATION_INFO = 0x1a,
        SB_REG_MANUFACTURE_DATE = 0x1b,
        SB_REG_SERIAL_NUMBER = 0x1c,
        SB_REG_MANUFACTURER_NAME = 0x20,
        SB_REG_DEVICE_NAME = 0x21,
        SB_REG_DEVICE_CHEMISTRY = 0x22,
        SB_REG_MANUFACTURER_DATA = 0x23,
        SB_REG_OPTIONAL_MFG_FUNCTION_5 = 0x2f,
        SB_REG_OPTIONAL_MFG_FUNCTION_4 = 0x3c,
        SB_REG_OPTIONAL_MFG_FUNCTION_3 = 0x3d,
        SB_REG_OPTIONAL_MFG_FUNCTION_2 = 0x3e,
        SB_REG_OPTIONAL_MFG_FUNCTION_1 = 0x3f,
        SB_REG_SB_PACK_STATUS = 0x43,
        SB_REG_SB_ALT_MANUFACTURER_ACCESS = 0x44,
        SB_REG_LAST
};

struct sb_register {
        /* Register name */
        char name[32];
        /* Address of the register. */
        enum sb_register_address address;
        /* Length of the data. */
        uint8_t length;
        /* Data. */
        uint8_t data[SB_REG_DATA_MAX_SIZE];
        /* Is the register read only */
        bool is_read_only;
        /* Is the register a word/block*/
        bool is_word;
        /* Register successful read count */
        uint32_t successful_read_count;
        /* Register failed read count */
        uint32_t failed_read_count;
        /* Register successful write count */
        uint32_t successful_write_count;
        /* Register failed write count */
        uint32_t failed_write_count;
};

/** Read a register value of a smart battery
 */
int sb_read_register(enum sb_register_address address, uint8_t *buffer, uint8_t *buf_size);

/** Write register value of a smart battery
 */
int sb_write_register(enum sb_register_address address, uint8_t *buffer, size_t buf_size);

/** Update smart battery registers based on PAC, temp data.
 */
void sb_update_registers(void);

/* Writes the data of the sb_register to the flash
 * EEPROM Emulation Type B uses a 2 bytes key to address 4 bytes of data
 * The MSB of the key will contain the address of the register and the LSB of the key will contain the offset of the
 * data. When the LSB of the key is n = 0, the data will represent the length of sb_register->data. When the LSB of the
 * key is n > 0, the data will represent the concatenation of sb_register->data[2^n]...sb_reigster->data[2^n + 3]. For
 * example, if we have the register SB_REG_DEVICE NAME = { address = 0x21, length = 5, data = { 0x44, 0x6F, 0x6C, 0x6F,
 * 0x73 } }:
 * +-------------------+------------------+---------------------------------+
 * | Key MSB (Address) | Key LSB (offset) | 4 Bytes of Data (Concatenation) |
 * +-------------------+------------------+---------------------------------+
 * | 0x21              |                0 | 5                               |
 * | 0x21              |                1 | 0x6F6C6F44                      |
 * | 0x21              |                2 | 0xXXXXXX73                      |
 * +-------------------+------------------+---------------------------------+
 * Returns true if write was successful
 */
bool sb_write_register_data_to_dflash(enum sb_register_address address, uint8_t *data, uint32_t length);

/* Writes the smart battery register to the flash */
bool sb_write_register_to_dflash(enum sb_register_address address);

/* Reads sb_register data stored by sb_write_register_data_to_dflash
 * Returns true if read was successful
 */
bool sb_read_register_data_from_dflash(enum sb_register_address address, uint8_t *data, uint32_t *length);

/* Reads the smart battery register from the flash */
bool sb_read_register_from_dflash(enum sb_register_address address);

/* Handles the sbreg command from UART, returns true if succeeds */
bool sb_sbreg_command_handler(uint8_t address, char register_type, size_t length, uint8_t *data);

/* Prints the Smart Battery register data stored in flash */
void sb_print_register_from_dflash(uint8_t *cmd_data);

/* Returns true if register is read only */
bool sb_is_register_read_only(enum sb_register_address address);

/* Returns true if register size is word */
bool sb_is_register_word(enum sb_register_address address);

/* Prints the data of a specific smart battery register */
void sb_print_sb_register(enum sb_register_address address);

/* Prints the data of the smart battery registers */
void sb_print_sb_registers(void);

/* Loads All the Smart Battery register data stored into the flash */
void sb_load_registers_from_dflash(void);

/* Calls the register handler for the SMBus read commands, returns the number of bytes sent by SMBus target */
int sb_register_smbus_read_handler(enum sb_register_address address);

/* Calls the register handler for the SMBus write commands, returns the number of bytes read from SMBus target */
int sb_register_smbus_write_handler(enum sb_register_address address);

#endif /* SMART_BATTERY_H_ */
