/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SB_CONFIG_EEPROM_LAYOUT_H
#define SB_CONFIG_EEPROM_LAYOUT_H

#include <stdint.h>

/* Battery functions
 * Copied from src/platform/ec/include/battery_smart.h
 */
#define SB_MANUFACTURER_ACCESS 0x00
#define SB_REMAINING_CAPACITY_ALARM 0x01
#define SB_REMAINING_TIME_ALARM 0x02
#define SB_BATTERY_MODE 0x03
#define SB_AT_RATE 0x04
#define SB_AT_RATE_TIME_TO_FULL 0x05
#define SB_AT_RATE_TIME_TO_EMPTY 0x06
#define SB_AT_RATE_OK 0x07
#define SB_TEMPERATURE 0x08
#define SB_VOLTAGE 0x09
#define SB_CURRENT 0x0a
#define SB_AVERAGE_CURRENT 0x0b
#define SB_MAX_ERROR 0x0c
#define SB_RELATIVE_STATE_OF_CHARGE 0x0d
#define SB_ABSOLUTE_STATE_OF_CHARGE 0x0e
#define SB_REMAINING_CAPACITY 0x0f
#define SB_FULL_CHARGE_CAPACITY 0x10
#define SB_RUN_TIME_TO_EMPTY 0x11
#define SB_AVERAGE_TIME_TO_EMPTY 0x12
#define SB_AVERAGE_TIME_TO_FULL 0x13
#define SB_CHARGING_CURRENT 0x14
#define SB_CHARGING_VOLTAGE 0x15
#define SB_BATTERY_STATUS 0x16
#define SB_CYCLE_COUNT 0x17
#define SB_DESIGN_CAPACITY 0x18
#define SB_DESIGN_VOLTAGE 0x19
#define SB_SPECIFICATION_INFO 0x1a
#define SB_MANUFACTURE_DATE 0x1b
#define SB_SERIAL_NUMBER 0x1c
#define SB_MANUFACTURER_NAME 0x20
#define SB_DEVICE_NAME 0x21
#define SB_DEVICE_CHEMISTRY 0x22
#define SB_MANUFACTURER_DATA 0x23
#define SB_OPTIONAL_MFG_FUNC1 0x3C
#define SB_OPTIONAL_MFG_FUNC2 0x3D
#define SB_OPTIONAL_MFG_FUNC3 0x3E
#define SB_OPTIONAL_MFG_FUNC4 0x3F
/* Extension of smart battery spec, may not be supported on all platforms */
#define SB_PACK_STATUS 0x43
#define SB_ALT_MANUFACTURER_ACCESS 0x44
#define SB_MANUFACTURE_INFO 0x70

#define STATIC_ASSERT_STRUCT_SIZE(struct_name, size)    \
	typedef char assert_struct_##struct_name##_size \
		[(sizeof(struct struct_name) == size) ? 1 : -1]

#define SB_TOTAL_BLOCK_REGISTERS 7
#define SB_TOTAL_WORD_REGISTERS 170

#define SB_BLOCK_REGISTER_MAX_LENGTH 32
struct sb_block_register {
	/* Length of the data.
	 * 0 means this register is not present in the smarty battery.
	 */
	uint8_t length;
	uint8_t data[SB_BLOCK_REGISTER_MAX_LENGTH];
} __attribute__((packed));
STATIC_ASSERT_STRUCT_SIZE(sb_block_register, 33);

struct sb_word_register {
	uint8_t present;
	uint16_t data;
} __attribute__((packed));
STATIC_ASSERT_STRUCT_SIZE(sb_word_register, 3);

struct eeprom_data {
	uint8_t version; /* Version of this data structure. */
	uint8_t polarity; /* Polarity, can be 2-float, 1-high, 0-low */
	uint8_t manufactured_year; /* Manufactured year. */
	uint8_t manufactured_week; /* Manufactured week. */
	uint16_t serial_number; /* Serial Number of the cable. */
	uint32_t crc; /* CRC from next field to end of the structure. */
	uint8_t reserved_1[246]; /* Reserved bytes - should be set to 0. */

	struct sb_block_register block_registers[SB_TOTAL_BLOCK_REGISTERS];
	uint8_t reserved_2[25]; /* Reserved bytes - should be set to 0. */

	struct sb_word_register word_registers[SB_TOTAL_WORD_REGISTERS];
	uint8_t reserved_3[2]; /* Reserved bytes - should be set to 0. */
} __attribute__((packed));
STATIC_ASSERT_STRUCT_SIZE(eeprom_data, 1024);

/** Returns offset in to eeprom_data.word_registers[] for given SB register
 * address
 */
static inline uint8_t sb_get_word_register_offset(uint8_t address)
{
	return address;
}

/** Returns offset in to eeprom_data.word_registers[] for given SB register
 * address
 */
static inline uint8_t sb_get_block_register_offset(uint8_t address)
{
	uint8_t map[] = { SB_MANUFACTURER_NAME,	      SB_DEVICE_NAME,
			  SB_DEVICE_CHEMISTRY,	      SB_MANUFACTURER_DATA,
			  SB_ALT_MANUFACTURER_ACCESS, SB_MANUFACTURE_INFO };
	for (int i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (map[i] == address) {
			return i;
		}
	}
	// not found hence return error;
	return 0xff;
}

static inline uint8_t sb_is_valid_address(uint8_t address)
{
	uint8_t map[] = { SB_MANUFACTURER_ACCESS,
			  SB_REMAINING_CAPACITY_ALARM,
			  SB_REMAINING_TIME_ALARM,
			  SB_BATTERY_MODE,
			  SB_AT_RATE,
			  SB_AT_RATE_TIME_TO_FULL,
			  SB_AT_RATE_TIME_TO_EMPTY,
			  SB_AT_RATE_OK,
			  SB_TEMPERATURE,
			  SB_VOLTAGE,
			  SB_CURRENT,
			  SB_AVERAGE_CURRENT,
			  SB_MAX_ERROR,
			  SB_RELATIVE_STATE_OF_CHARGE,
			  SB_ABSOLUTE_STATE_OF_CHARGE,
			  SB_REMAINING_CAPACITY,
			  SB_FULL_CHARGE_CAPACITY,
			  SB_RUN_TIME_TO_EMPTY,
			  SB_AVERAGE_TIME_TO_EMPTY,
			  SB_AVERAGE_TIME_TO_FULL,
			  SB_CHARGING_CURRENT,
			  SB_CHARGING_VOLTAGE,
			  SB_BATTERY_STATUS,
			  SB_CYCLE_COUNT,
			  SB_DESIGN_CAPACITY,
			  SB_DESIGN_VOLTAGE,
			  SB_SPECIFICATION_INFO,
			  SB_MANUFACTURE_DATE,
			  SB_SERIAL_NUMBER,
			  SB_MANUFACTURER_NAME,
			  SB_DEVICE_NAME,
			  SB_DEVICE_CHEMISTRY,
			  SB_MANUFACTURER_DATA,
			  SB_OPTIONAL_MFG_FUNC1,
			  SB_OPTIONAL_MFG_FUNC2,
			  SB_OPTIONAL_MFG_FUNC3,
			  SB_OPTIONAL_MFG_FUNC4,
			  SB_PACK_STATUS,
			  SB_ALT_MANUFACTURER_ACCESS,
			  SB_MANUFACTURE_INFO };
	for (int i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (map[i] == address) {
			return 1;
		}
	}
	// error;
	return 0;
}

#endif // SB_CONFIG_EEPROM_LAYOUT_H
