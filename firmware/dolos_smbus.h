/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_SMBUS_H_
#define DOLOS_SMBUS_H_

#include <stdint.h>

#include "ti/smbus/smbus.h"

/** Initialize the SMBus on controller and target.
 */
void dsb_init(void);

/** Read a 1-byte response from the given target.
 *
 */
int dsb_controller_read_byte(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data);

/** Read a 1-word response from the given target.
 *
 */
int dsb_controller_read_word(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data);

/** Read response(one or byte byte) from the given target.
 */
int dsb_controller_read_block(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data, size_t *rx_len);

/** Write zero or more byte to the given target.
 */
int dsb_controller_write(uint8_t target_address, uint8_t command_byte, uint8_t *tx_data, int tx_size);

/** Writes word to the target SMBus instance
 */
void dsb_target_write_word(const uint8_t *data);

/** Writes block to the target SMBus instance
 */
void dsb_target_write_block(const uint8_t *data, const uint8_t length);

/** Reads word from the target SMBus instance
 */
void dsb_target_read_word(uint8_t *data);

/** Reads block from the target SMBus instance
 */
void dsb_target_read_block(uint8_t *data, uint8_t *length);

/* Rests Dolos target SMBus */
void dsb_target_reset(void);

/* Sets the communication state on the SMBus target interface (Used by DOLOS_SMBUS_COMMUNICATION_TIMER) */
void dsb_target_set_communication_state(bool state);

/* Gets the communication state on the SMBus target interface */
bool dsb_target_get_communication_state(void);

#endif /* DOLOS_SMBUS_H_ */
