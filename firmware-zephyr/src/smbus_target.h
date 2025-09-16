/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SMBUS_TARGET_H_
#define SMBUS_TARGET_H_

#include <stdbool.h>

enum smbus_target_comm_state {
	SMBUS_READ_WORD,
	SMBUS_READ_BLOCK,
	SMBUS_WRITE_WORD,
	SMBUS_WRITE_BLOCK,
	SMBUS_INVALID,
};

int smbus_target_init(void);

/**
 * @brief Checks if there was communication in the last 10 seconds
 *
 * @return True if there was communication, false otherwise
 */
bool smbus_target_is_comms_detected_long(void);

/**
 * @brief Checks if there was communication in the last 2 seconds
 *
 * @return True if there was communication, false otherwise
 */
bool smbus_target_is_comms_detected_short(void);

#endif /* SMBUS_TARGET_H_ */
