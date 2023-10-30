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

#define CMD_PRINT_STATS "print-stats"
#define CMD_PRINT_SMBUS_STATS "print-smbus-stats"
#define CMD_PRINT_GPIO_STATS "print-gpio-stats"
#define CMD_PRINT_UART_STATS "print-uart-stats"
#define CMD_SBREG "sbreg"

void cmd_handle(void)
{
        uint16_t cmd_len;
        char *cmd_line;

        cmd_line = (char *)uart_get_pending_data(&cmd_len);
        if (cmd_line == NULL) {
                return;
        }

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
        } else {
                printf("unknown command");
        }

        uart_mark_data_processed();
}
