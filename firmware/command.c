/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "uart.h"
#include "printf.h"
#include "stats.h"
#include "smart_battery.h"
#include "log.h"
#include "pac.h"
#include "dolos_gpio.h"
#include "dconfig.h"

#include "cli.h"

cli_t cli;
static cli_status_t cmd_help_func(int argc, char **argv);

static cli_status_t cmd_stats_func(int argc, char **argv)
{
        if (argc <= 1) {
                print_stats();
                return CLI_OK;
        }

        if (!strcasecmp(argv[1], "gpio")) {
                print_gpio_stats();
                return CLI_OK;
        }

        if (!strcasecmp(argv[1], "uart")) {
                print_uart_stats();
                return CLI_OK;
        }

        if (!strcasecmp(argv[1], "smbus")) {
                print_smbus_stats();
                return CLI_OK;
        }

        return CLI_E_INVALID_ARGS;
}

static cli_status_t cmd_pac_func(int argc, char **argv)
{
        pac_print_readings();
        return CLI_OK;
}

static cli_status_t cmd_temp_func(int argc, char **argv)
{
        dgpio_print_temp_readings();
        return CLI_OK;
}

static cli_status_t cmd_log_level_func(int argc, char **argv)
{
        if (argc != 2) {
                printf("set-log-level requires one of the following as argument - debug, info, error");
                return CLI_E_INVALID_ARGS;
        }
        if (!strcasecmp(argv[1], "debug")) {
                log_set_level(LOG_LEVEL_DEBUG);
        } else if (!strcasecmp(argv[1], "info")) {
                log_set_level(LOG_LEVEL_INFO);
        } else if (!strcasecmp(argv[1], "error")) {
                log_set_level(LOG_LEVEL_ERROR);
        } else {
                printf("set-log-level requires one of the following as argument - debug, info, error");
                return CLI_E_INVALID_ARGS;
        }

        return CLI_OK;
}

static cli_status_t cmd_system_present_func(int argc, char **argv)
{
        bool polarity;
        if (argc == 1) {
                polarity = dconfig_get_system_present_polarity();
                if (polarity) {
                        printf("Current SYSTEM_PRESENT polarity: Active HIGH with pull-down resistor\r\n");
                } else {
                        printf("Current SYSTEM_PRESENT polarity: Active LOW with pull-up resistor\r\n");
                }
                return CLI_OK;
        }
        if (!strcmp(argv[1], "0")) {
                polarity = false;
        } else {
                polarity = true;
        }
        if (polarity) {
                printf("Setting SYSTEM_PRESENT to Active HIGH \r\n");
        } else {
                printf("Setting SYSTEM_PRESENT to Active LOW \r\n");
        }

        dconfig_set_system_present_polarity(polarity);
        return CLI_OK;
}

static cli_status_t cmd_read_flash_func(int argc, char **argv)
{
        printf("Not implemented\n");
        return CLI_OK;
}

static cli_status_t cmd_sbreg_func(int argc, char **argv)
{
        printf("Not implemented\n");
        return CLI_OK;
}

cmd_t cmd_tbl[] = {
        { .cmd = "help", .help = "Prints this help menu", .func = cmd_help_func },
        { .cmd = "stats", .help = "Print statistics. (options - gpio, uart, smbus)", .func = cmd_stats_func },
        { .cmd = "pac", .help = "Print PAC readings.", .func = cmd_pac_func },
        { .cmd = "temp", .help = "Print temperature readings.", .func = cmd_temp_func },
        { .cmd = "log-level", .help = "Set log level(error, info, debug).", .func = cmd_log_level_func },
        { .cmd = "system-present", .help = "Change SYSTEM PRESENT polarity.", .func = cmd_system_present_func },
        { .cmd = "read-flash", .help = "Read from flash register values.", .func = cmd_read_flash_func },
        { .cmd = "sbreg", .help = "Set Smart battery register values.", .func = cmd_sbreg_func },
};

static cli_status_t cmd_help_func(int argc, char **argv)
{
        int i;
        printf("Supported Commands:\n\r");
        for (i = 0; i < sizeof(cmd_tbl) / sizeof(cmd_tbl[0]); i++) {
                printf("%20s - %s\r\n", cmd_tbl[i].cmd, cmd_tbl[i].help);
        }

        return CLI_OK;
}

void cmd_init(void)
{
        cli.cmd_tbl = cmd_tbl;
        cli.cmd_cnt = sizeof(cmd_tbl) / sizeof(cmd_t);
        cli_init(&cli);
}

void cmd_putc(char data)
{
        cli_put(&cli, data);
}

void cmd_process(void)
{
        cli_process(&cli);
}
