/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "uart.h"
#include "printf.h"
#include "stats.h"
#include "smart_battery.h"
#include "log.h"
#include "pac.h"
#include "dolos_gpio.h"

#define CMD_PRINT_STATS "print-stats"
#define CMD_PRINT_SMBUS_STATS "print-smbus-stats"
#define CMD_PRINT_GPIO_STATS "print-gpio-stats"
#define CMD_PRINT_UART_STATS "print-uart-stats"
#define CMD_SBREG "sbreg"
#define CMD_LOG_LEVEL_DEBUG "log-level-debug"
#define CMD_LOG_LEVEL_INFO "log-level-info"
#define CMD_LOG_LEVEL_ERROR "log-level-error"
#define CMD_PRINT_PAC_READINGS "print-pac-readings"
#define CMD_PRINT_TEMP_READINGS "print-temp-readings"
#define CMD_PRINT_REGISTER_FROM_FLASH "print-register-from-flash"

void cmd_handle(void)
{
        uint16_t cmd_len;
        char *cmd_line;

        cmd_line = (char *)uart_get_pending_data(&cmd_len);
        if (cmd_line == NULL) {
                return;
        }
        stats.cmd_rx_count++;

        if (!strncmp(cmd_line, CMD_PRINT_STATS, sizeof(CMD_PRINT_STATS) - 1)) {
                print_stats();
        } else if (!strncmp(cmd_line, CMD_PRINT_SMBUS_STATS, sizeof(CMD_PRINT_SMBUS_STATS) - 1)) {
                print_smbus_stats();
        } else if (!strncmp(cmd_line, CMD_PRINT_GPIO_STATS, sizeof(CMD_PRINT_GPIO_STATS) - 1)) {
                print_gpio_stats();
        } else if (!strncmp(cmd_line, CMD_PRINT_UART_STATS, sizeof(CMD_PRINT_UART_STATS) - 1)) {
                print_uart_stats();
        } else if (!strncmp(cmd_line, CMD_SBREG, sizeof(CMD_SBREG) - 1)) {
                sb_sbreg_command_handler((uint8_t *)(cmd_line + sizeof(CMD_SBREG) - 1));
        } else if (!strncmp(cmd_line, CMD_LOG_LEVEL_DEBUG, sizeof(CMD_LOG_LEVEL_DEBUG) - 1)) {
                log_set_level(LOG_LEVEL_DEBUG);
        } else if (!strncmp(cmd_line, CMD_LOG_LEVEL_INFO, sizeof(CMD_LOG_LEVEL_INFO) - 1)) {
                log_set_level(LOG_LEVEL_INFO);
        } else if (!strncmp(cmd_line, CMD_LOG_LEVEL_ERROR, sizeof(CMD_LOG_LEVEL_ERROR) - 1)) {
                log_set_level(LOG_LEVEL_ERROR);
        } else if (!strncmp(cmd_line, CMD_PRINT_PAC_READINGS, sizeof(CMD_PRINT_PAC_READINGS) - 1)) {
                pac_print_readings();
        } else if (!strncmp(cmd_line, CMD_PRINT_TEMP_READINGS, sizeof(CMD_PRINT_TEMP_READINGS) - 1)) {
                dgpio_print_temp_readings();
        } else if (!strncmp(cmd_line, CMD_PRINT_REGISTER_FROM_FLASH, sizeof(CMD_PRINT_REGISTER_FROM_FLASH) - 1)) {
                sb_print_register_from_dflash((uint8_t *)(cmd_line + sizeof(CMD_PRINT_REGISTER_FROM_FLASH) - 1));
        } else {
                stats.cmd_inval_count++;
                printf("unknown command %s\n", cmd_line);
        }

        uart_mark_data_processed();
}
