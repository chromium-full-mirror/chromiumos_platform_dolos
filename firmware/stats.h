/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef STATS_H_
#define STATS_H_

#include <ti/smbus/smbus.h>

struct stats {
        uint32_t smbus_target_int_count;
        uint32_t smbus_target_reg_read_bytes;
        uint32_t smbus_target_state_count[SMBus_State_Unknown + 1];
        uint32_t smbus_target_reg_read_success;
        uint32_t smbus_target_reg_read_failures;
};

extern struct stats stats;

/** Print statistics on the UART.
 */
void print_stats(void);

#endif /* STATS_H_ */
