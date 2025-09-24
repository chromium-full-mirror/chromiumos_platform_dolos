/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"
#include "eeprom_layout.h"
#include "error.h"
#include "pac.h"
#include "smart_battery.h"
#include "smbus_target.h"
#include "temperature.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(smart_battery, LOG_LEVEL_ERR);

#define SB_BATTERY_MODE_CAPACITY_MODE_MASK (1 << 15)
#define WORD_SIZE 2

static uint32_t design_capacity_10mWh;
static uint32_t design_capacity_mAh;
static uint32_t remaining_capacity_10mWh;
static uint32_t remaining_capacity_mAh;
static uint32_t full_charge_capacity_10mWh;
static uint32_t full_charge_capacity_mAh;

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

	if (reg == NULL || reg->length == 0 || size == NULL) {
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

	if (reg == NULL || reg->length == 0) {
		return -EINVAL;
	}

	reg->length = size;

	return 0;
}

static int sb_curr_reg_read_word(uint8_t offset, uint8_t *data)
{
	struct sb_word_register *reg = eeprom_get_word_reg(offset);

	if (reg == NULL || !reg->present || data == NULL) {
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

	if (reg == NULL || reg->length == 0 || data == NULL) {
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

	if (reg == NULL || !reg->present) {
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

	if (reg == NULL || reg->length == 0xff) {
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

static int sb_reg_read_word(uint8_t offset, uint8_t *buf, uint8_t buf_size)
{
	struct sb_word_register *reg = eeprom_get_word_reg(offset);

	if (reg == NULL || !reg->present || buf == NULL) {
		return -EINVAL;
	}

	if (buf_size < WORD_SIZE) {
		return -EOVERFLOW;
	}

	memcpy(buf, &reg->data, WORD_SIZE);
	return WORD_SIZE;
}

static int sb_reg_read_block(uint8_t offset, uint8_t *buf, uint8_t buf_size)
{
	struct sb_block_register *reg = eeprom_get_block_reg(offset);

	if (reg == NULL || reg->length == 0 || buf == NULL) {
		return -EINVAL;
	}

	if (buf_size < reg->length) {
		return -EOVERFLOW;
	}

	memcpy(buf, reg->data, reg->length);
	return reg->length;
}

int sb_reg_read(uint8_t addr, uint8_t *buf, uint8_t buf_size)
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

	if (reg == NULL || !reg->present || buf == NULL) {
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

	if (reg == NULL || reg->length == 0 || buf == NULL || buf_size == 0) {
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
	// The PAC1954 and SB_CURRENT have opposite current polarity
	int16_t current_ma = pac1954_get_current_ma();
	current_ma = -current_ma;
	uint16_t temperature = temperature_get_k() * 10;
	sb_reg_write(SB_VOLTAGE, (uint8_t *)&voltage_mv, sizeof(voltage_mv));
	sb_reg_write(SB_CURRENT, (uint8_t *)&current_ma, sizeof(current_ma));
	sb_reg_write(SB_TEMPERATURE, (uint8_t *)&temperature,
		     sizeof(temperature));
}

int sb_reg_is_read_only(uint8_t addr, bool *is_read_only)
{
	if (!sb_is_valid_address(addr) || is_read_only == NULL) {
		return -EINVAL;
	}

	*is_read_only = false;
	return 0;
}

int sb_reg_is_word(uint8_t addr)
{
	if (!sb_is_valid_address(addr)) {
		return -EINVAL;
	}

	return (sb_get_word_register_offset(addr) == 0xff);
}

/**
 * @brief Returns the CAPACITY_MODE bit from SB_BATTERY_MODE register.
 * When CAPACITY_MODE bit is 0, smart battery will report capacity in mAh.
 * When CAPACITY_MODE bit is 1, smart battery will report capacity in 10mWh.
 *
 * @param capacity_mode Pointer to the CAPACITY_MODE bit;
 *
 * @retval 0 on success
 * @retval -EINVAL if the register does not exist
 */
static int sb_get_capacity_mode(uint8_t *capacity_mode)
{
	uint16_t batt_mode;
	int ret;

	if (capacity_mode == NULL) {
		return -EINVAL;
	}

	ret = sb_reg_read(SB_BATTERY_MODE, (uint8_t *)&batt_mode,
			  sizeof(batt_mode));
	if (ret < 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to get CAPACITY_MODE from SB_BATTERY_MODE register, err=%d",
			ret);
		return ret;
	}

	*capacity_mode = (batt_mode & SB_BATTERY_MODE_CAPACITY_MODE_MASK) ? 1 :
									    0;

	return 0;
}

/**
 * @brief Calculates the capacity in mAh and 10mWh according to CAPACITY_MODE
 * bit in SB_BATTERY_MODE register
 *
 * @retval 0 on success
 * @retval -EINVAL if the register does not exist
 */
static int sb_reg_calc_capacity(void)
{
	uint8_t capacity_mode;
	uint32_t design_voltage = 0;
	uint32_t design_capacity = 0;
	uint32_t remaining_capacity = 0;
	uint32_t full_charge_capacity = 0;
	int ret;

	ret = sb_get_capacity_mode(&capacity_mode);
	if (ret < 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to calculate smart battery capacity, could not read CAPACITY_MODE, err=%d",
			ret);
		return ret;
	}

	ret = sb_reg_read(SB_DESIGN_VOLTAGE, (uint8_t *)&design_voltage,
			  sizeof(design_voltage));
	if (ret < 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to calculate smart battery capacity, could not read SB_DESIGN_VOLTAGE, err=%d",
			ret);
		return ret;
	}

	ret = sb_reg_read(SB_DESIGN_CAPACITY, (uint8_t *)&design_capacity,
			  sizeof(design_capacity));
	if (ret < 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to calculate smart battery capacity, could not read SB_DESIGN_CAPACITY, err=%d",
			ret);
		return ret;
	}

	ret = sb_reg_read(SB_REMAINING_CAPACITY, (uint8_t *)&remaining_capacity,
			  sizeof(remaining_capacity));
	if (ret < 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to calculate smart battery capacity, could not read SB_REMAINING_CAPACITY, err=%d",
			ret);
		return ret;
	}

	ret = sb_reg_read(SB_FULL_CHARGE_CAPACITY,
			  (uint8_t *)&full_charge_capacity,
			  sizeof(full_charge_capacity));
	if (ret < 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to calculate smart battery capacity, could not read SB_FULL_CHARGE_CAPACITY, err=%d",
			ret);
		return ret;
	}

	if (capacity_mode) {
		/* Smart battery has 10mWh, should calulcate mAh */
		design_capacity_10mWh = design_capacity;
		design_capacity_mAh = design_capacity * 10000 / design_voltage;

		remaining_capacity_10mWh = remaining_capacity;
		remaining_capacity_mAh =
			remaining_capacity * 10000 / design_voltage;

		full_charge_capacity_10mWh = full_charge_capacity;
		full_charge_capacity_mAh =
			full_charge_capacity * 10000 / design_voltage;
	} else {
		/* Smart battery has mAh, should calulcate 10mWh */
		design_capacity_10mWh =
			design_capacity * design_voltage / 10000;
		design_capacity_mAh = design_capacity;

		remaining_capacity_10mWh =
			remaining_capacity * design_voltage / 10000;
		remaining_capacity_mAh = remaining_capacity;

		full_charge_capacity_10mWh =
			full_charge_capacity * design_voltage / 10000;
		full_charge_capacity_mAh = full_charge_capacity;
	}

	return 0;
}

int sb_init(void)
{
	int ret;

	ret = sb_reg_calc_capacity();
	if (ret < 0) {
		DOLOS_LOG_ERR(ERROR_SMART_BATTERY, ret,
			      "Failed to initialize smart battery, err=%d",
			      ret);
		return ret;
	}

	return 0;
}

uint16_t sb_get_design_capacity_mah(void)
{
	return design_capacity_mAh;
}

/**
 * @brief Updates capacity registers after writing CAPACITY_MODE bit
 */
static int sb_battery_mode_write_handler(void)
{
	int ret;
	uint8_t capacity_mode;

	ret = sb_get_capacity_mode(&capacity_mode);
	if (ret < 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to execute SB_BATTERY_MODE write handler, couldn't read CAPACITY_MODE bit, err=%d",
			ret);
		return ret;
	}

	if (capacity_mode) {
		/* 10mWh */
		ret = sb_reg_write(SB_DESIGN_CAPACITY,
				   (uint8_t *)&design_capacity_10mWh,
				   sizeof(uint16_t));
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_SMART_BATTERY, ret,
				"Failed to execute SB_BATTERY_MODE write handler, couldn't update SB_DESIGN_CAPACITY to 10mWh unit, err=%d",
				ret);
			return ret;
		}
		ret = sb_reg_write(SB_REMAINING_CAPACITY,
				   (uint8_t *)&remaining_capacity_10mWh,
				   sizeof(uint16_t));
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_SMART_BATTERY, ret,
				"Failed to execute SB_BATTERY_MODE write handler, couldn't update SB_REMAINING_CAPACITY to 10mWh unit, err=%d",
				ret);
			return ret;
		}
		ret = sb_reg_write(SB_FULL_CHARGE_CAPACITY,
				   (uint8_t *)&full_charge_capacity_10mWh,
				   sizeof(uint16_t));
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_SMART_BATTERY, ret,
				"Failed to execute SB_BATTERY_MODE write handler, couldn't update SB_FULL_CHARGE_CAPACITY to 10mWh unit, err=%d",
				ret);
			return ret;
		}
	} else {
		/* mAh */
		ret = sb_reg_write(SB_DESIGN_CAPACITY,
				   (uint8_t *)&design_capacity_mAh,
				   sizeof(uint16_t));
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_SMART_BATTERY, ret,
				"Failed to execute SB_DESIGN_CAPACITY write handler, couldn't update SB_FULL_CHARGE_CAPACITY to mAh unit, err=%d",
				ret);
			return ret;
		}
		ret = sb_reg_write(SB_REMAINING_CAPACITY,
				   (uint8_t *)&remaining_capacity_mAh,
				   sizeof(uint16_t));
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_SMART_BATTERY, ret,
				"Failed to execute SB_REMAINING_CAPACITY write handler, couldn't update SB_FULL_CHARGE_CAPACITY to mAh unit, err=%d",
				ret);
			return ret;
		}
		ret = sb_reg_write(SB_FULL_CHARGE_CAPACITY,
				   (uint8_t *)&full_charge_capacity_mAh,
				   sizeof(uint16_t));
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_SMART_BATTERY, ret,
				"Failed to execute SB_FULL_CHARGE_CAPACITY write handler, couldn't update SB_FULL_CHARGE_CAPACITY to mAh unit, err=%d",
				ret);
			return ret;
		}
	}

	return 0;
}

void sb_smbus_read_write_handler(enum smbus_target_comm_state state)
{
	int ret = 0;

	if (state == SMBUS_WRITE_WORD) {
		switch (sb_curr_reg_addr) {
		case SB_BATTERY_MODE:
			ret = sb_battery_mode_write_handler();
			break;
		default:
			break;
		}
	}

	if (ret != 0) {
		DOLOS_LOG_ERR(
			ERROR_SMART_BATTERY, ret,
			"Failed to execute smart battery smbus handler, err=%d",
			ret);
	}
}

/* Smart battery update registers thread info */
#define SB_UPDATE_REGISTERS_THREAD_STACK_SIZE 512
#define SB_UPDATE_REGISTERS_THREAD_PRIORITY 0

/* Smart battery update registers thread handler */
static void sb_update_registers_thread_fn(void *p1, void *p2, void *p3)
{
	while (true) {
		sb_update_registers();
		k_sleep(K_SECONDS(1));
	}
}

/* Starting smart battery update registers thread */
K_THREAD_DEFINE(sb_update_registers_thread,
		SB_UPDATE_REGISTERS_THREAD_STACK_SIZE,
		sb_update_registers_thread_fn, NULL, NULL, NULL,
		SB_UPDATE_REGISTERS_THREAD_PRIORITY, 0, 0);

static inline void cmd_sb_reg_print(const struct shell *sh, uint8_t addr)
{
	uint8_t offset = sb_get_block_register_offset(addr);
	if (offset == 0xff) {
		struct sb_word_register *reg = eeprom_get_word_reg(addr);
		if (reg == NULL) {
			shell_print(sh, "Failed to read register");
			return;
		}
		shell_print(sh, "Data (word): 0x%04x", reg->data);
	} else {
		struct sb_block_register *reg = eeprom_get_block_reg(offset);
		if (reg == NULL) {
			shell_print(sh, "Failed to read block");
			return;
		}
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
