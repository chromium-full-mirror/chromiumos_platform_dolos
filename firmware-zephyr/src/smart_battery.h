/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SMART_BATTERY_H_
#define SMART_BATTERY_H_

#include "smbus_target.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SB_REG_DATA_MAX_SIZE 32

/**
 * @brief Sets the current sb_curr_reg_addr to register with the given address.
 * Meant to be used by the SMBus target
 *
 * @param addr Register address
 *
 * @retval 0 on success
 * @retval -EOVERFLOW if the register does not exist
 */
int sb_curr_reg_ptr_set(uint8_t addr);

/**
 * @brief Get the sb_curr_reg_addr block register data length. Meant to be used
 * by the SMBus target
 *
 * @param length Pointer to the register length
 *
 * @retval 0 on success
 * @retval -EINVAL if the register is a word or block register is not present
 */
int sb_curr_reg_get_length(uint8_t *length);

/**
 * @brief Set the sb_curr_reg_addr block register data length. Meant to be used
 * by the SMBus target
 *
 * @param length Register length
 *
 * @retval 0 on success
 * @retval -EINVAL if the register is a word or block register is not present
 */
int sb_curr_reg_set_length(uint8_t length);

/**
 * @brief Reads the current byte from the sb_curr_reg_addr register. Meant to be
 * used by the SMBus target
 *
 * @param data Pointer to byte data
 *
 * @retval 0 on success
 * @retval -EINVAL if register is not present
 * @retval -EOVERFLOW if there's no more data to be read
 */
int sb_curr_reg_read(uint8_t *data);

/**
 * @brief Writes the current byte to the sb_curr_reg_addr register. Meant to be
 * used by the SMBus target
 *
 * @param data Byte data
 *
 * @retval 0 on success
 * @retval -EINVAL if register is not present
 * @retval -EOVERFLOW if no more data can be written
 */
int sb_curr_reg_write(uint8_t data);

/**
 * @brief Checks if the register sb_curr_reg_addr is a word. Meant to be used by
 * the SMBus target
 *
 * @return True if the register is a word, false otherwise
 */
bool sb_curr_reg_is_word(void);

/**
 * @brief Checks if the register sb_curr_reg_addr is read only. Meant to be used
 * by the SMBus target
 *
 * @return True if the register is read only, false otherwise
 */
bool sb_curr_reg_is_read_only(void);

/**
 * @brief Reads the value of a smart battery register
 *
 * @param addr Register address
 * @param data Pointer to the register data buffer
 * @param buf_size Register data buffer size
 *
 * @retval Number of bytes read on success
 * @retval -EINVAL if the register does not exist or is not present
 * @retval -EOVERFLOW if the buffer is not large enough
 */
int sb_reg_read(uint8_t addr, uint8_t *buf, uint8_t buf_size);

/**
 * @brief Writes the value in the buffer to a smart battery register
 *
 * @param addr Register address
 * @param buf Pointer to the register data buffer
 * @param buf_size Register data buffer size
 *
 * @retval 0 on success
 * @retval -EINVAL if the register does not exist or is not present
 * @retval -EOVERFLOW if the register data is too large
 */
int sb_reg_write(uint8_t addr, const uint8_t *buf, uint8_t buf_size);

/**
 * @brief Check if the smart battery register is read only
 *
 * @param addr Register address
 * @param is_read_only Pointer to the read only boolean
 *
 * @retval 0 on success
 * @retval -EINVAL if the register does not exist
 */
int sb_reg_is_read_only(uint8_t addr, bool *is_read_only);

/**
 * @brief Check if the smart battery register is word
 *
 * @param addr Register address
 *
 * @retval 0 on success
 * @retval -EINVAL if the register does not exist
 */
int sb_reg_is_word(uint8_t addr);

/**
 * @brief Initializes smart battery
 *
 * @retval 0 on success
 * @retval Negative on failure
 */
int sb_init(void);

/**
 * @brief Returns the design capacity in mAh
 *
 * @return Returns the design capacity in mAh
 */
uint16_t sb_get_design_capacity_mah(void);

/**
 * @brief Executes special read/write handlers after receiving a stop condition.
 * Meant to be used by SMBus target.
 *
 * @param state smbus_target_comm_state state type
 */
void sb_smbus_read_write_handler(enum smbus_target_comm_state state);

#endif /* SMART_BATTERY_H_ */
