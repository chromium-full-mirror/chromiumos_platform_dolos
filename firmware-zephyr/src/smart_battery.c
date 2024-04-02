/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"
#include "eeprom_layout.h"
#include "pac.h"
#include "smart_battery.h"
#include "temperature.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(smart_battery, LOG_LEVEL_ERR);

static uint8_t sb_curr_reg_addr = 0;
static uint8_t sb_curr_reg_data_idx = 0;

int sb_curr_reg_ptr_set(uint8_t addr)
{
	if (!sb_is_valid_address(addr)) {
		return -EOVERFLOW;
	}

	sb_curr_reg_addr = addr;
	sb_curr_reg_data_idx = 0;

	return 0;
}

int sb_curr_reg_get_length(uint8_t *size)
{
	uint8_t offset = sb_get_block_register_offset(sb_curr_reg_addr);

	/* Register is a word */
	if (offset == 0xff) {
		return -EINVAL;
	}

	struct sb_block_register *reg = eeprom_get_block_reg(offset);

	if (reg->length == 0) {
		return -EINVAL;
	}

	*size = reg->length;

	return 0;
}

int sb_curr_reg_set_length(uint8_t size)
{
	uint8_t offset = sb_get_block_register_offset(sb_curr_reg_addr);

	/* Register is a word */
	if (offset == 0xff) {
		return -EINVAL;
	}

	struct sb_block_register *reg = eeprom_get_block_reg(offset);

	if (reg->length == 0) {
		return -EINVAL;
	}

	reg->length = size;

	return 0;
}

static int sb_curr_reg_read_word(uint8_t offset, uint8_t *data)
{
	struct sb_word_register *reg = eeprom_get_word_reg(offset);

	if (!reg->present) {
		return -EINVAL;
	}

	if (sb_curr_reg_data_idx >= sizeof(reg->data)) {
		return -EOVERFLOW;
	}

	*data = *(((uint8_t *)&reg->data) + sb_curr_reg_data_idx);
	sb_curr_reg_data_idx++;
	return 0;
}

static int sb_curr_reg_read_block(uint8_t offset, uint8_t *data)
{
	struct sb_block_register *reg = eeprom_get_block_reg(offset);

	if (reg->length == 0) {
		return -EINVAL;
	}

	if (sb_curr_reg_data_idx >= reg->length) {
		return -EOVERFLOW;
	}

	*data = reg->data[sb_curr_reg_data_idx++];
	return 0;
}

int sb_curr_reg_read(uint8_t *data)
{
	uint8_t offset = sb_get_block_register_offset(sb_curr_reg_addr);

	if (offset == 0xff) {
		return sb_curr_reg_read_word(sb_curr_reg_addr, data);
	} else {
		return sb_curr_reg_read_block(offset, data);
	}
}

static int sb_curr_reg_write_word(uint8_t offset, uint8_t data)
{
	struct sb_word_register *reg = eeprom_get_word_reg(offset);

	if (!reg->present) {
		return -EINVAL;
	}

	if (sb_curr_reg_data_idx >= sizeof(reg->data)) {
		return -EOVERFLOW;
	}

	*(((uint8_t *)&reg->data) + sb_curr_reg_data_idx) = data;
	sb_curr_reg_data_idx++;
	return 0;
}

static int sb_curr_reg_write_block(uint8_t offset, uint8_t data)
{
	struct sb_block_register *reg = eeprom_get_block_reg(offset);

	if (reg->length == 0xff) {
		return -EINVAL;
	}

	if (sb_curr_reg_data_idx >= reg->length) {
		return -EOVERFLOW;
	}

	reg->data[sb_curr_reg_data_idx++] = data;
	return 0;
}

int sb_curr_reg_write(uint8_t data)
{
	uint8_t offset = sb_get_block_register_offset(sb_curr_reg_addr);

	if (offset == 0xff) {
		return sb_curr_reg_write_word(sb_curr_reg_addr, data);
	} else {
		return sb_curr_reg_write_block(offset, data);
	}
}

bool sb_curr_reg_is_word(void)
{
	return (sb_get_block_register_offset(sb_curr_reg_addr) == 0xff);
}

bool sb_curr_reg_is_read_only(void)
{
	return false;
}

static int sb_reg_read_word(uint8_t offset, const uint8_t *buf,
			    uint8_t *buf_size)
{
	struct sb_word_register *reg = eeprom_get_word_reg(offset);

	if (!reg->present) {
		buf = NULL;
		*buf_size = 0;
		return -EINVAL;
	}

	buf = (uint8_t *)&reg->data;
	*buf_size = sizeof(reg->data);
	return 0;
}

static int sb_reg_read_block(uint8_t offset, const uint8_t *buf,
			     uint8_t *buf_size)
{
	struct sb_block_register *reg = eeprom_get_block_reg(offset);

	if (reg->length == 0) {
		buf = NULL;
		*buf_size = 0;
		return -EINVAL;
	}

	buf = reg->data;
	*buf_size = reg->length;
	return 0;
}

int sb_reg_read(uint8_t addr, const uint8_t *buf, uint8_t *buf_size)
{
	if (!sb_is_valid_address(addr)) {
		return -EINVAL;
	}

	uint8_t offset = sb_get_block_register_offset(addr);

	if (offset == 0xff) {
		return sb_reg_read_word(addr, buf, buf_size);
	} else {
		return sb_reg_read_block(offset, buf, buf_size);
	}
}

static int sb_reg_write_word(uint8_t offset, const uint8_t *buf,
			     uint8_t buf_size)
{
	struct sb_word_register *reg = eeprom_get_word_reg(offset);

	if (!reg->present) {
		return -EINVAL;
	}

	if (buf_size != sizeof(reg->data)) {
		return -EOVERFLOW;
	}

	reg->data = *((uint16_t *)buf);
	return 0;
}

static int sb_reg_write_block(uint8_t offset, const uint8_t *buf,
			      uint8_t buf_size)
{
	struct sb_block_register *reg = eeprom_get_block_reg(offset);

	if (reg->length == 0 || buf_size == 0) {
		return -EINVAL;
	}

	if (buf_size > SB_REG_DATA_MAX_SIZE) {
		return -EOVERFLOW;
	}

	memcpy(reg->data, buf, buf_size);
	reg->length = buf_size;
	return 0;
}

int sb_reg_write(uint8_t addr, const uint8_t *buf, uint8_t buf_size)
{
	if (!sb_is_valid_address(addr)) {
		return -EINVAL;
	}

	uint8_t offset = sb_get_block_register_offset(addr);

	if (offset == 0xff) {
		return sb_reg_write_word(addr, buf, buf_size);
	} else {
		return sb_reg_write_block(offset, buf, buf_size);
	}
}

void sb_update_registers(void)
{
	uint16_t voltage_mv = pac1954_get_voltage_mv();
	uint16_t current_ma = pac1954_get_current_ma();
	uint16_t temperature = temperature_get_k() * 10;
	sb_reg_write(SB_VOLTAGE, (uint8_t *)&voltage_mv, sizeof(voltage_mv));
	sb_reg_write(SB_CURRENT, (uint8_t *)&current_ma, sizeof(current_ma));
	sb_reg_write(SB_TEMPERATURE, (uint8_t *)&temperature,
		     sizeof(temperature));
}

int sb_reg_is_read_only(uint8_t addr, bool *is_read_only)
{
	if (!sb_is_valid_address(addr)) {
		return -EINVAL;
	}

	*is_read_only = false;
	return 0;
}

int sb_reg_is_word(uint8_t addr, bool *is_word)
{
	if (!sb_is_valid_address(addr)) {
		return -EINVAL;
	}

	return (sb_get_word_register_offset(addr) == 0xff);
}

int sb_reg_load(void)
{
	return 0;
}

static inline void cmd_sb_reg_print(const struct shell *sh, uint8_t addr)
{
	uint8_t offset = sb_get_block_register_offset(addr);
	if (offset == 0xff) {
		struct sb_word_register *reg = eeprom_get_word_reg(addr);
		shell_print(sh, "Data (word): 0x%04x", reg->data);
	} else {
		struct sb_block_register *reg = eeprom_get_block_reg(offset);
		shell_print(sh, "Data (block): length=%d", reg->length);
		shell_hexdump(sh, reg->data, reg->length);
	}
}

static int cmd_sb_reg_print_handler(const struct shell *sh, size_t argc,
				    char **argv)
{
	if (argc > 1) {
		int err = 0;
		uint8_t addr;

		addr = shell_strtoul(argv[1], 16, &err);
		if (err != 0) {
			shell_error(sh, "Invaild address, addr=%s", argv[1]);
			return 0;
		}

		if (!sb_is_valid_address(addr)) {
			shell_error(sh, "Register addr=0x%02x was not found",
				    addr);
			return 0;
		}

		cmd_sb_reg_print(sh, addr);

	} else {
		size_t arr_size;
		struct sb_word_register *word_regs;
		struct sb_block_register *block_regs;

		word_regs = eeprom_get_word_reg_arr(&arr_size);
		for (size_t i = 0; i < arr_size; i++) {
			if (!word_regs[i].present) {
				continue;
			}
			shell_print(sh, "Data (word): 0x%04x",
				    word_regs[i].data);
		}

		block_regs = eeprom_get_block_reg_arr(&arr_size);
		for (size_t i = 0; i < arr_size; i++) {
			if (block_regs[i].length == 0) {
				continue;
			}
			shell_print(sh, "Data (block): length=%d",
				    block_regs[i].length);
			shell_hexdump(sh, block_regs[i].data,
				      block_regs[i].length);
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
