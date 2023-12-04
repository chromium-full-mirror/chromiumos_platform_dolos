/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PERF_H_
#define PERF_H_

#include <stdint.h>
#include <math.h>

#include "ti_msp_dl_config.h"

#define MCLK_BITS 5

struct call_site_measurement {
        /* Number of times a method is invoked */
        uint32_t invoked_count;
        /* Total cycle count during method execution */
        uint64_t total_cycles;
        /* Max cycle count during method execution */
        uint64_t max_cycles;
        /* Time in microseconds since the beginning of program when a method is invoked, used for performance
         * measurements */
        uint64_t total_time_us;
        /* Max time in microseconds during method execution */
        uint64_t max_time_us;
};

struct call_site_measurements {
        struct call_site_measurement irq_temp_adc;
        struct call_site_measurement irq_uart;
        struct call_site_measurement irq_target_smbus;
};

extern struct call_site_measurements call_site_measurements;

/* Prints the performance measurements */
void print_perf_measurements(void);

#define MAX(_a, _b) (_a > _b ? _a : _b)

/* Starts performance measurements */
#define PERF_RECORD_START(_site)                      \
        call_site_measurements._site.invoked_count++; \
        DL_SYSTICK_resetValue();                      \
        DL_SYSTICK_enable();

/* Ends performance measurements */
#define PERF_RECORD_END(_site)                                                                               \
        DL_SYSTICK_disable();                                                                                \
        uint64_t total_cycle = DL_SYSTICK_getPeriod() - DL_SYSTICK_getValue();                               \
        uint64_t total_time = total_cycle >> MCLK_BITS;                                                      \
        call_site_measurements._site.total_cycles += total_cycle;                                            \
        call_site_measurements._site.total_time_us += total_time;                                            \
        call_site_measurements._site.max_cycles = MAX(total_cycle, call_site_measurements._site.max_cycles); \
        call_site_measurements._site.max_time_us = MAX(total_time, call_site_measurements._site.max_time_us);

#endif /* PERF_H_ */
