/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_SMBUS_H_
#define DOLOS_SMBUS_H_

#include <ti/smbus/smbus.h>
#include <stdint.h>

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
int dsb_controller_read_block(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data);

/** Write zero or more byte to the given target.
 */
int dsb_controller_write(uint8_t target_address, uint8_t command_byte, uint8_t *tx_data, int tx_size);

#endif /* DOLOS_SMBUS_H_ */
