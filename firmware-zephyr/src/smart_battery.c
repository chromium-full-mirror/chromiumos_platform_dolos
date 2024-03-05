/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"
#include "pac.h"
#include "smart_battery.h"
#include "temperature.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(smart_battery, LOG_LEVEL_DBG);

/* Smart battery word registers */
#define SB_REG_MANUFACTURER_ACCESS_OFFSET 0x0c
#define SB_REG_REMAINING_CAPACITY_ALARM_OFFSET 0x0e
#define SB_REG_REMAINING_TIME_ALARM_OFFSET 0x10
#define SB_REG_BATTERY_MODE_OFFSET 0x12
#define SB_REG_AT_RATE_OFFSET 0x14
#define SB_REG_AT_RATE_TIME_TO_FULL_OFFSET 0x16
#define SB_REG_AT_RATE_TIME_TO_EMPTY_OFFSET 0x18
#define SB_REG_AT_RATE_OK_OFFSET 0x1a
#define SB_REG_TEMPERATURE_OFFSET 0x1c
#define SB_REG_VOLTAGE_OFFSET 0x1e
#define SB_REG_CURRENT_OFFSET 0x20
#define SB_REG_AVERAGE_CURRENT_OFFSET 0x22
#define SB_REG_MAX_ERROR_OFFSET 0x24
#define SB_REG_RELATIVE_STATE_OF_CHARGE_OFFSET 0x26
#define SB_REG_ABSOLUTE_STATE_OF_CHARGE_OFFSET 0x28
#define SB_REG_REMAINING_CAPACITY_OFFSET 0x2a
#define SB_REG_FULL_CHARGE_CAPACITY_OFFSET 0x2c
#define SB_REG_RUN_TIME_TO_EMPTY_OFFSET 0x2e
#define SB_REG_AVERAGE_TIME_TO_EMPTY_OFFSET 0x30
#define SB_REG_AVERAGE_TIME_TO_FULL_OFFSET 0x32
#define SB_REG_CHARGING_CURRENT_OFFSET 0x34
#define SB_REG_CHARGING_VOLTAGE_OFFSET 0x36
#define SB_REG_BATTERY_STATUS_OFFSET 0x38
#define SB_REG_CYCLE_COUNT_OFFSET 0x3a
#define SB_REG_DESIGN_CAPACITY_OFFSET 0x3c
#define SB_REG_DESIGN_VOLTAGE_OFFSET 0x3e
#define SB_REG_SPECIFICATION_INFO_OFFSET 0x40
#define SB_REG_MANUFACTURE_DATE_OFFSET 0x42
#define SB_REG_SERIAL_NUMBER_OFFSET 0x44
#define SB_REG_OPTIONAL_MFG_FUNCTION_5_OFFSET 0x46
#define SB_REG_OPTIONAL_MFG_FUNCTION_4_OFFSET 0x48
#define SB_REG_OPTIONAL_MFG_FUNCTION_3_OFFSET 0x4a
#define SB_REG_OPTIONAL_MFG_FUNCTION_2_OFFSET 0x4c
#define SB_REG_OPTIONAL_MFG_FUNCTION_1_OFFSET 0x4e
#define SB_REG_SB_PACK_STATUS_OFFSET 0x50

/* Smart battery block registers */
#define SB_REG_MANUFACTURER_NAME_OFFSET 0x52
#define SB_REG_DEVICE_NAME_OFFSET 0x73
#define SB_REG_DEVICE_CHEMISTRY_OFFSET 0x94
#define SB_REG_MANUFACTURER_DATA_OFFSET 0xb5
#define SB_REG_SB_ALT_MANUFACTURER_ACCESS_OFFSET 0xd6

#define DEF_READONLY_WORD_SB_REG(_address, ...)                             \
	{                                                                   \
		.name = #_address, .addr = SB_REG_##_address, .length = 2,  \
		.data = __VA_ARGS__, .is_read_only = true, .is_word = true, \
		.eeprom_offset = SB_REG_##_address##_OFFSET                 \
	}

#define DEF_READONLY_BLOCK_SB_REG(_address, _length, ...)                     \
	{                                                                     \
		.name = #_address, .addr = SB_REG_##_address,                 \
		.length = _length, .data = __VA_ARGS__, .is_read_only = true, \
		.is_word = false, .eeprom_offset = SB_REG_##_address##_OFFSET \
	}

#define DEF_READWRITE_WORD_SB_REG(_address, ...)                             \
	{                                                                    \
		.name = #_address, .addr = SB_REG_##_address, .length = 2,   \
		.data = __VA_ARGS__, .is_read_only = false, .is_word = true, \
		.eeprom_offset = SB_REG_##_address##_OFFSET                  \
	}

static struct sb_reg sb_registers[] = {
	DEF_READWRITE_WORD_SB_REG(MANUFACTURER_ACCESS, { 0x83, 0x43 }),
	DEF_READWRITE_WORD_SB_REG(REMAINING_CAPACITY_ALARM, { 0x67, 0x02 }),
	DEF_READWRITE_WORD_SB_REG(REMAINING_TIME_ALARM, { 0x0a, 0x00 }),
	DEF_READWRITE_WORD_SB_REG(BATTERY_MODE, { 0x01, 0x60 }),
	DEF_READWRITE_WORD_SB_REG(AT_RATE, { 0x0, 0x0 }),

	DEF_READONLY_WORD_SB_REG(AT_RATE_TIME_TO_FULL, { 0xff, 0xff }),
	DEF_READONLY_WORD_SB_REG(AT_RATE_TIME_TO_EMPTY, { 0xff, 0xff }),
	DEF_READONLY_WORD_SB_REG(AT_RATE_OK, { 0x1, 0x0 }),
	DEF_READONLY_WORD_SB_REG(TEMPERATURE, { 0x24, 0x0c }),
	DEF_READONLY_WORD_SB_REG(VOLTAGE, { 0xd4, 0x20 }),
	DEF_READONLY_WORD_SB_REG(CURRENT, { 0x83, 0x0f }),
	DEF_READONLY_WORD_SB_REG(AVERAGE_CURRENT, { 0xf1, 0x09 }),
	DEF_READONLY_WORD_SB_REG(MAX_ERROR, { 0x01, 0x00 }),
	DEF_READONLY_WORD_SB_REG(RELATIVE_STATE_OF_CHARGE, { 0x63, 0x00 }),
	DEF_READONLY_WORD_SB_REG(ABSOLUTE_STATE_OF_CHARGE, { 0x58, 0x00 }),
	DEF_READONLY_WORD_SB_REG(REMAINING_CAPACITY, { 0x16, 0x11 }),
	DEF_READONLY_WORD_SB_REG(FULL_CHARGE_CAPACITY, { 0x2c, 0x15 }),
	DEF_READONLY_WORD_SB_REG(RUN_TIME_TO_EMPTY, { 0xff, 0xff }),
	DEF_READONLY_WORD_SB_REG(AVERAGE_TIME_TO_EMPTY, { 0xff, 0xff }),
	DEF_READONLY_WORD_SB_REG(AVERAGE_TIME_TO_FULL, { 0xff, 0xff }),

	DEF_READWRITE_WORD_SB_REG(CHARGING_CURRENT, { 0x0, 0x0 }),
	DEF_READWRITE_WORD_SB_REG(CHARGING_VOLTAGE, { 0x0, 0x0 }),

	DEF_READONLY_WORD_SB_REG(BATTERY_STATUS, { 0xe0, 0x00 }),
	DEF_READONLY_WORD_SB_REG(CYCLE_COUNT, { 0x21, 0x00 }),
	DEF_READONLY_WORD_SB_REG(DESIGN_CAPACITY, { 0x06, 0x18 }),
	DEF_READONLY_WORD_SB_REG(DESIGN_VOLTAGE, { 0x14, 0x1e }),
	DEF_READONLY_WORD_SB_REG(SPECIFICATION_INFO, { 0x31, 0x00 }),
	DEF_READONLY_WORD_SB_REG(MANUFACTURE_DATE, { 0x5b, 0x52 }),
	DEF_READONLY_WORD_SB_REG(SERIAL_NUMBER, { 0x9b, 0x00 }),

	DEF_READONLY_BLOCK_SB_REG(MANUFACTURER_NAME, 11,
				  { 0x33, 0x33, 0x33, 0x2d, 0x41, 0x43, 0x2d,
				    0x44, 0x41, 0x2D, 0x41 }),
	DEF_READONLY_BLOCK_SB_REG(DEVICE_NAME, 9,
				  { 0x47, 0x48, 0x30, 0x32, 0x30, 0x34, 0x37,
				    0x58, 0x4c }),
	DEF_READONLY_BLOCK_SB_REG(DEVICE_CHEMISTRY, 4,
				  { 0x4c, 0x49, 0x4f, 0x4e }),
	DEF_READONLY_BLOCK_SB_REG(MANUFACTURER_DATA, 16,
				  { 0x36, 0x4B, 0x51, 0x44, 0x43, 0x30, 0x41,
				    0x38, 0x31, 0x45, 0x56, 0x4B, 0x5A, 0x4A,
				    0x00, 0x01 }),

	DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_5, { 0x00, 0x00 }),
	DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_4, { 0x00, 0x00 }),
	DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_3, { 0x00, 0x00 }),
	DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_2, { 0x10, 0x83 }),
	DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_1, { 0x10, 0x84 }),
	DEF_READONLY_WORD_SB_REG(SB_PACK_STATUS, { 0x1, 0x00 }),

	DEF_READONLY_BLOCK_SB_REG(SB_ALT_MANUFACTURER_ACCESS, 6,
				  { 0x54, 0x00, 0x87, 0x03, 0x00, 0x00 }),

};

static struct sb_reg *sb_curr_reg_ptr = &sb_registers[0];
static uint8_t *sb_curr_reg_data = &sb_registers[0].data[0];

/**
 * @brief Find smart_battery register in sb_registers array for given register
 * addr
 *
 * @param addr Register address
 *
 * @retval Pointer to the register
 * @retval NULL if register was not found
 */
static struct sb_reg *sb_reg_find(enum sb_reg_addr addr)
{
	if (addr >= SB_REG_LAST) {
		return NULL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(sb_registers); i++) {
		if (sb_registers[i].addr == addr) {
			return &sb_registers[i];
		}
	}

	return NULL;
}

int sb_curr_reg_ptr_set(uint8_t addr)
{
	struct sb_reg *reg = sb_reg_find(addr);

	if (reg == NULL) {
		LOG_DBG("Register not found");
		return -EOVERFLOW;
	}

	sb_curr_reg_ptr = reg;
	sb_curr_reg_data = reg->data;

	return 0;
}

int sb_curr_reg_get_length(uint8_t *size)
{
	if (sb_curr_reg_ptr->is_word) {
		return -EINVAL;
	}

	*size = sb_curr_reg_ptr->length;
	return 0;
}

int sb_curr_reg_set_length(uint8_t size)
{
	if (sb_curr_reg_ptr->is_word) {
		return -EINVAL;
	}

	sb_curr_reg_ptr->length = size;
	return 0;
}

int sb_curr_reg_read(uint8_t *data)
{
	size_t bytes_read = sb_curr_reg_data - &sb_curr_reg_ptr->data[0];

	if (bytes_read >= sb_curr_reg_ptr->length ||
	    bytes_read >= SB_REG_DATA_MAX_SIZE) {
		return -EOVERFLOW;
	}

	*data = *sb_curr_reg_data;
	++sb_curr_reg_data;

	return 0;
}

int sb_curr_reg_write(uint8_t data)
{
	size_t bytes_wrote = sb_curr_reg_data - sb_curr_reg_ptr->data;

	if (bytes_wrote >= sb_curr_reg_ptr->length ||
	    bytes_wrote >= SB_REG_DATA_MAX_SIZE) {
		return -EOVERFLOW;
	}

	*sb_curr_reg_data = data;
	++sb_curr_reg_data;

	return 0;
}

bool sb_curr_reg_is_word(void)
{
	return sb_curr_reg_ptr->is_word;
}

bool sb_curr_reg_is_read_only(void)
{
	return sb_curr_reg_ptr->is_read_only;
}

int sb_reg_read(enum sb_reg_addr addr, const struct sb_reg **reg)
{
	*reg = sb_reg_find(addr);

	if (*reg == NULL) {
		return -EOVERFLOW;
	}

	return 0;
}

int sb_reg_write(enum sb_reg_addr addr, uint8_t *buf, uint8_t buf_size)
{
	struct sb_reg *reg;

	reg = sb_reg_find(addr);
	if (reg == NULL) {
		return -EINVAL;
	}

	if (buf_size > SB_REG_DATA_MAX_SIZE) {
		return -EOVERFLOW;
	}

	if (reg->is_word) {
		memcpy(reg->data, buf, 2);
	} else {
		reg->length = buf_size;
		memcpy(reg->data, buf, buf_size);
	}

	return 0;
}

void sb_update_registers(void)
{
	uint16_t voltage_mv = pac1954_get_voltage_mv();
	uint16_t current_ma = pac1954_get_current_ma();
	uint16_t temperature = temperature_get_k() * 10;
	sb_reg_write(SB_REG_VOLTAGE, (uint8_t *)&voltage_mv,
		     sizeof(voltage_mv));
	sb_reg_write(SB_REG_CURRENT, (uint8_t *)&current_ma,
		     sizeof(current_ma));
	sb_reg_write(SB_REG_TEMPERATURE, (uint8_t *)&temperature,
		     sizeof(temperature));
}

int sb_reg_is_read_only(enum sb_reg_addr addr, bool *is_read_only)
{
	struct sb_reg *sb_reg = sb_reg_find(addr);

	if (sb_reg != NULL) {
		return -EOVERFLOW;
	}

	*is_read_only = sb_reg->is_read_only;
	return 0;
}

int sb_reg_is_word(enum sb_reg_addr addr, bool *is_word)
{
	struct sb_reg *sb_reg = sb_reg_find(addr);

	if (sb_reg != NULL) {
		return -EOVERFLOW;
	}

	*is_word = sb_reg->is_word;
	return 0;
}

static void cmd_sb_reg_print(const struct shell *sh, const struct sb_reg *reg)
{
	shell_print(sh, "Register: %s", reg->name);

	if (reg->is_word) {
		shell_print(sh, "Data (word): 0x%04x", sys_get_le16(reg->data));
	} else {
		shell_print(sh, "Data (block): length=%d", reg->length);
		shell_hexdump(sh, reg->data, reg->length);
	}
}

int sb_reg_load(void)
{
	int ret;
	struct sb_reg *reg;
	uint8_t buf[32];
	uint8_t len;

	for (size_t i = 0; i < ARRAY_SIZE(sb_registers); i++) {
		reg = &sb_registers[i];

		if (reg->is_word) {
			ret = eeprom_get_word_reg(reg->eeprom_offset,
						  (uint16_t *)buf);

		} else {
			ret = eeprom_get_block_reg(reg->eeprom_offset, &len,
						   buf);
		}

		if (ret != 0) {
			LOG_ERR("Failed to load register name=%s addr=0x%02x from EEPROM",
				reg->name, reg->addr);
			return ret;
		}

		ret = sb_reg_write(reg->addr, buf, len);

		if (ret != 0) {
			LOG_ERR("Failed to store the data read from the EEPROM into register name=%s addr=0x%02x from EEPROM",
				reg->name, reg->addr);
			return ret;
		}

		LOG_DBG("Loaded register name=%s addr=0x%02x from EEPROM",
			reg->name, reg->addr);
	}

	return 0;
}

static int cmd_sb_reg_print_handler(const struct shell *sh, size_t argc,
				    char **argv)
{
	enum sb_reg_addr addr;
	const struct sb_reg *reg = NULL;
	int ret;

	if (argc > 1) {
		addr = strtol(argv[1], NULL, 16);
		ret = sb_reg_read(addr, &reg);

		if (ret != 0) {
			shell_error(sh, "Register addr=0x%02x was not found",
				    addr);
			return 0;
		}
		shell_print(sh, "reg name=%s", reg->name);
		cmd_sb_reg_print(sh, reg);

	} else {
		for (size_t i = 0; i < ARRAY_SIZE(sb_registers); i++) {
			cmd_sb_reg_print(sh, &sb_registers[i]);
		}
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sb_sub_cmds,
			       SHELL_CMD_ARG(print, NULL,
					     "Prints smart battery registers",
					     cmd_sb_reg_print_handler, 1, 1),
			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(sb, &sb_sub_cmds, "Smart battery command set", NULL);
