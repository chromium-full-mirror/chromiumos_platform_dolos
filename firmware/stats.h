/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef STATS_H_
#define STATS_H_

#include "ti/smbus/smbus.h"

struct stats {
        uint32_t smbus_target_int_count;
        uint32_t smbus_target_reg_read_bytes;
        uint32_t smbus_target_reg_write_bytes;
        uint32_t smbus_target_state_count[SMBus_State_Unknown + 1];
        uint32_t smbus_target_reg_read_success;
        uint32_t smbus_target_reg_read_failures;
        uint32_t smbus_target_reg_write_success;
        uint32_t smbus_target_reg_write_failures;
        uint32_t dgpio_gpioa_bq_chrg_ok_int_count;
        uint32_t dgpio_gpioa_system_present_int_count;
        uint32_t dgpio_gpiob_efuse_pg_int_count;
        uint32_t uart_rx_int_count;
        uint32_t uart_tx_int_count;
        uint32_t uart_rx_char_count;
        uint32_t uart_rx_scan_count;
        uint32_t uart_rx_eof_count;
        uint32_t cmd_rx_count;
        uint32_t cmd_inval_count;
        uint32_t dflash_write_success;
        uint32_t dflash_write_failure;
        uint32_t dflash_read_failure;
        uint32_t dflash_read_success;
};

extern struct stats stats;

/** Print all statistics.
 */
void print_stats(void);

/** Print SMBUS statistics.
 */
void print_smbus_stats(void);

/** Print GPIO statistics.
 */
void print_gpio_stats(void);

/** Print UART statistics.
 */
void print_uart_stats(void);

/** Print command handling statistics.
 */
void print_cmd_stats(void);

/** Print Flash statistics.
 */
void print_flash_stats(void);

#endif /* STATS_H_ */
