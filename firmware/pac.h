/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PAC_H_
#define PAC_H_

#include <stdint.h>

/** Initializes PAC.
 */
int pac_init(void);

/** Refresh PAC registers and obtain recent values.
 */
int pac_refresh(void);

/** Read voltage value from PAC in millivolts.
 */
uint32_t pac_read_voltage_mv(void);

/** Read current value from PAC in milliamp.
 */
uint32_t pac_read_current_ma(void);

/* Prints PAC readings to the UART interface */
void pac_print_readings(void);

#endif /* PAC_H_ */
