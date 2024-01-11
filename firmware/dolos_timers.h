/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_TIMERS_H_
#define DOLOS_TIMERS_H_

#include "ti_msp_dl_config.h"

/* Initializes Dolos timer modules */
void dtimers_init(void);

/* Returns time passed since boot in 100ms granularity */
uint32_t dtimers_get_op_timer_elapsed(void);

/* Gets Dolos uptime */
void dtimers_get_uptime(uint8_t *seconds, uint8_t *minutes, uint8_t *hours, uint32_t *days);

/* Resets the DOLOS_SMBUS_COMMUNICATION_TIMER*/
void dtimers_reset_smbus_communication_timer(void);

#endif /* DOLOS_TIMERS_H_ */
