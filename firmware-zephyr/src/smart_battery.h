/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SMART_BATTERY_H_
#define SMART_BATTERY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SB_REG_DATA_MAX_SIZE 32

enum sb_reg_addr {
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

struct sb_reg {
	/* Register name */
	char name[32];
	/* Address of the register. */
	enum sb_reg_addr addr;
	/* Length of the data. */
	uint8_t length;
	/* Data. */
	uint8_t data[SB_REG_DATA_MAX_SIZE];
	/* Is the register read only */
	bool is_read_only;
	/* Is the register a word/block*/
	bool is_word;
	/* EEPROM offset */
	uint8_t eeprom_offset;
};

/**
 * @brief Sets the current sb_curr_reg_ptr to register with the given address.
 * Meant to be used by the SMBus target
 *
 * @param addr Register address
 *
 * @retval 0 on success
 * @retval -EOVERFLOW if the register does not exist
 */
int sb_curr_reg_ptr_set(uint8_t addr);

/**
 * @brief Get the sb_curr_reg_ptr block register data length. Meant to be used
 * by the SMBus target
 *
 * @param length Pointer to the register length
 *
 * @retval 0 on success
 * @retval -EINVAL if the register is a word
 */
int sb_curr_reg_get_length(uint8_t *length);

/**
 * @brief Set the sb_curr_reg_ptr block register data length. Meant to be used
 * by the SMBus target
 *
 * @param length Register length
 *
 * @retval 0 on success
 * @retval -EINVAL if the register is a word
 */
int sb_curr_reg_set_length(uint8_t length);

/**
 * @brief Reads the current byte from the sb_curr_reg_ptr register. Meant to be
 * used by the SMBus target
 *
 * @param data Pointer to byte data
 *
 * @retval 0 on success
 * @retval -EOVERFLOW if the register has no more data to send
 */
int sb_curr_reg_read(uint8_t *data);

/**
 * @brief Writes the current byte to the sb_curr_reg_ptr register. Meant to be
 * used by the SMBus target
 *
 * @param data Byte data
 *
 * @retval 0 on success
 * @retval -EOVERFLOW if the register length/max length exceeded
 */
int sb_curr_reg_write(uint8_t data);

/**
 * @brief Checks if the register sb_curr_reg_ptr is a word. Meant to be used by
 * the SMBus target
 *
 * @return True if the register is a word, false otherwise
 */
bool sb_curr_reg_is_word(void);

/**
 * @brief Checks if the register sb_curr_reg_ptr is read only. Meant to be used
 * by the SMBus target
 *
 * @return True if the register is read only, false otherwise
 */
bool sb_curr_reg_is_read_only(void);

/**
 * @brief Reads the value of a smart battery register
 *
 * @param addr Register address
 * @param buf Pointer to the register data buffer
 * @param buf_size Pointer to the register data buffer length (NULL if register
 * is word)
 * @param is_word Pointer to boolean indicating if the register is word or block
 *
 * @retval 0 on success
 * @retval -EOVERFLOW if the register does not exist
 */
int sb_reg_read(enum sb_reg_addr addr, const struct sb_reg **reg);

/**
 * @brief Reads the value of a smart battery register
 *
 * @param addr Register address
 * @param buf Pointer to the register data buffer
 * @param buf_size Register data buffer length (no effect if register is word)
 *
 * @retval 0 on success
 * @retval -EINVAL if the register does not exist
 * @retval -EOVERFLOW if the register data is too large
 */
int sb_reg_write(enum sb_reg_addr addr, uint8_t *buffer, uint8_t buf_size);

/**
 * @brief Check if the smart battery register is read only
 *
 * @param addr Register address
 * @param is_read_only Pointer to the read only boolean
 *
 * @retval 0 on success
 * @retval -EOVERFLOW if the register does not exist
 */
int sb_reg_is_read_only(enum sb_reg_addr addr, bool *is_read_only);

/**
 * @brief Check if the smart battery register is word
 *
 * @param addr Register address
 * @param is_word Pointer to the word boolean
 *
 * @retval 0 on success
 * @retval -EOVERFLOW if the register does not exist
 */
int sb_reg_is_word(enum sb_reg_addr, bool *is_word);

/**
 * @brief Loads the smart batter registers from the EEPROM
 *
 * @retval 0 on success
 * @retval Negative on failure
 */
int sb_reg_load(void);

#endif /* SMART_BATTERY_H_ */
