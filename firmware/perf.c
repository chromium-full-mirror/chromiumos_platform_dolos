/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "perf.h"
#include "printf.h"

struct call_site_measurements call_site_measurements = { 0 };

/* Prints the performance measurements for Temperature ADC */
static void print_perf_measurements_temp_adc(void)
{
        printf("Temperature ADC Performance Measurements:\r\n");
        printf("   Invoke Count   : %d\r\n", call_site_measurements.irq_temp_adc.invoked_count);
        printf("   Total Cycles   : %llu\r\n", call_site_measurements.irq_temp_adc.total_cycles);
        printf("   Max Cycles     : %llu\r\n", call_site_measurements.irq_temp_adc.max_cycles);
        printf("   Total Time (us): %llu\r\n", call_site_measurements.irq_temp_adc.total_time_us);
        printf("   Max Time (us)  : %llu\r\n", call_site_measurements.irq_temp_adc.max_time_us);
}

/* Prints the performance measurements for UART */
static void print_perf_measurements_uart(void)
{
        printf("UART Performance Measurements:\r\n");
        printf("   Invoke Count   : %d\r\n", call_site_measurements.irq_uart.invoked_count);
        printf("   Total Cycles   : %llu\r\n", call_site_measurements.irq_uart.total_cycles);
        printf("   Max Cycles     : %llu\r\n", call_site_measurements.irq_uart.max_cycles);
        printf("   Total Time (us): %llu\r\n", call_site_measurements.irq_uart.total_time_us);
        printf("   Max Time (us)  : %llu\r\n", call_site_measurements.irq_uart.max_time_us);
}

/* Prints the performance measurements for Target SMBus */
static void print_perf_measurements_target_smbus(void)
{
        printf("Target SMBus Performance Measurements:\r\n");
        printf("   Invoke Count   : %d\r\n", call_site_measurements.irq_target_smbus.invoked_count);
        printf("   Total Cycles   : %llu\r\n", call_site_measurements.irq_target_smbus.total_cycles);
        printf("   Max Cycles     : %llu\r\n", call_site_measurements.irq_target_smbus.max_cycles);
        printf("   Total Time (us): %llu\r\n", call_site_measurements.irq_target_smbus.total_time_us);
        printf("   Max Time (us)  : %llu\r\n", call_site_measurements.irq_target_smbus.max_time_us);
}

void print_perf_measurements(void)
{
        print_perf_measurements_temp_adc();
        print_perf_measurements_uart();
        print_perf_measurements_target_smbus();
}
