/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PAC_H_
#define PAC_H_

#include <stdint.h>

/**
 * @brief Gets the most recent voltage value from the PAC.
 *
 * @return Voltage in millivolt.
 */
uint32_t pac1954_get_voltage_mv(void);

/**
 * @brief Gets the most recent current value from the PAC.
 *
 * @return Current in milliampere.
 */
uint32_t pac1954_get_current_ma(void);

#endif /* PAC_H_ */
