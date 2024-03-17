/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SMBUS_TARGET_H_
#define SMBUS_TARGET_H_

#include <stdbool.h>

int smbus_target_init(void);

/**
 * @brief Checks if there was communication in the last 10 seconds
 *
 * @return True if there was communication, false otherwise
 */
bool smbus_target_is_communication_detected(void);

#endif /* SMBUS_TARGET_H_ */
