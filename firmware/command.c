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
#include "temperature.h"
#include "bms.h"
#include "cli.h"
#include "utils.h"
#include "bsl.h"
#include "perf.h"
#include "eeprom.h"
#include "error.h"

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
        temp_print_readings();
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
        uint8_t address;
        char register_type;
        size_t length;
        uint8_t data[SB_REG_DATA_MAX_SIZE];
        enum parse_status parse_status;

        if (argc < 4) {
                printf("Invalid arguments, usage: sbreg <address> {w|b|r} <length> <data...>\r\n");
                return CLI_E_INVALID_ARGS;
        }

        /* Read address byte */
        parse_status = parse_int(argv[1], (int *)&address);
        if (parse_status != PARSE_SUCCESS) {
                printf("Invalid address, must be 1 byte in integer format\r\n");
                return CLI_E_INVALID_ARGS;
        }

        /* Read command type */
        if (strcasecmp(argv[2], "w") && strcasecmp(argv[2], "b") && strcasecmp(argv[2], "r")) {
                printf("Register type should be w/b/r (word/block/reserved)\r\n");
                return CLI_E_INVALID_ARGS;
        }
        register_type = argv[2][0];

        /* Read command length */
        parse_status = parse_int(argv[3], (int *)&length);
        if (parse_status != PARSE_SUCCESS) {
                printf("Invalid length, must be an integer\r\n");
                return CLI_E_INVALID_ARGS;
        }

        if (length < 0 || length > SB_REG_DATA_MAX_SIZE) {
                printf("Invalid length, length should be between 0 and %d\r\n", SB_REG_DATA_MAX_SIZE);
        }

        if (argc != length + 4) {
                printf("Data length mismatch, should be equal to specified length\r\n");
                return CLI_E_INVALID_ARGS;
        }

        /* Read the rest of data */
        size_t i = 0;
        uint8_t current_byte;

        while (i < length) {
                parse_status = parse_int(argv[i + 4], (int *)&current_byte);
                if (parse_status != PARSE_SUCCESS) {
                        printf("Invalid data format, should be 1 byte in integer format\r\n");
                        return CLI_E_INVALID_ARGS;
                }
                data[i++] = current_byte;
        }

        /* Call sbreg handler in Smart Battery */
        if (!sb_sbreg_command_handler(address, register_type, length, data)) {
                printf("Failed to execute sbreg\r\n");
                return CLI_E_IO;
        }

        return CLI_OK;
}

static cli_status_t cmd_signal_func(int argc, char **argv)
{
        bms_print_state_trackers();
        return CLI_OK;
}

static cli_status_t cmd_output_func(int argc, char **argv)
{
        if (argc != 2) {
                printf("Argument required on or off\n\r");
                return CLI_E_INVALID_ARGS;
        }
        if (!strcasecmp(argv[1], "on")) {
                bms_force_output(true);
        } else {
                bms_force_output(false);
        }

        return CLI_OK;
}

static cli_status_t cmd_sb_print_func(int argc, char **argv)
{
        uint8_t address;
        enum parse_status parse_status;

        if (argc == 1) {
                sb_print_sb_registers();
                return CLI_OK;
        }

        /* Read address byte */
        parse_status = parse_int(argv[1], (int *)&address);
        if (parse_status != PARSE_SUCCESS) {
                printf("Invalid address, must be 1 byte in integer format\r\n");
                return CLI_E_INVALID_ARGS;
        }

        sb_print_sb_register(address);
        return CLI_OK;
}

static cli_status_t cmd_cli_echo_func(int argc, char **argv)
{
        cli_toggle_echo();
        return CLI_OK;
}

static cli_status_t cmd_bsl_invoke_func(int argc, char **argv)
{
        bsl_invoke();

        /* It won't return */
        return CLI_OK;
}

static cli_status_t cmd_perf_func(int argc, char **argv)
{
        print_perf_measurements();

        return CLI_OK;
}

static cli_status_t cmd_reset_smbus_func(int argc, char **argv)
{
        dsb_target_reset();

        return CLI_OK;
}

static cli_status_t cmd_eeprom_read_func(int argc, char **argv)
{
        if (argc == 1) {
                uint8_t data[EEPROM_MAX_SIZE];
                int ret;

                ret = eeprom_read_all(data);
                if (ret != DOLOS_SUCCESS) {
                        printf("Failed to read data from EEPROM, code: %d\r\n", ret);
                        return CLI_E_IO;
                }

                printf("EEPROM data:\r\n");
                for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
                        printf("%02X ", data[i]);

                        /* Print new line every 16 bytes */
                        if (((i + 1) & 0xF) == 0) {
                                printf("\r\n");
                        }
                }
        } else if (argc == 2) {
                int address;
                uint8_t data;
                int ret;

                if (parse_int(argv[1], &address) != PARSE_SUCCESS || address < 0 || address >= EEPROM_MAX_SIZE) {
                        printf("Incorrect address, must be a integer between 0 and %d\r\n", EEPROM_MAX_SIZE - 1);
                        return CLI_E_INVALID_ARGS;
                }

                ret = eeprom_read((uint8_t)address, &data);
                if (ret != DOLOS_SUCCESS) {
                        printf("Failed to read data from EEPROM, code: %d\r\n", ret);
                        return CLI_E_IO;
                }

                printf("Read EEPROM data at address %d = %02X\r\n", address, data);
        } else {
                printf("Usage: eeprom-read <address>\r\n");
        }
        return CLI_OK;
}

static cli_status_t cmd_eeprom_write_func(int argc, char **argv)
{
        if (argc == 3) {
                int address;
                int data;
                int ret;

                if (parse_int(argv[1], &address) != PARSE_SUCCESS || address < 0 || address >= EEPROM_MAX_SIZE) {
                        printf("Incorrect address, must be an integer between 0 and %d\r\n", EEPROM_MAX_SIZE - 1);
                        return CLI_E_INVALID_ARGS;
                }

                if (parse_int(argv[2], &data) != PARSE_SUCCESS || data < 0 || data > 255) {
                        printf("Incorrect data, must be a byte between 0 and 255\r\n");
                        return CLI_E_INVALID_ARGS;
                }

                ret = eeprom_write((uint8_t)address, (uint8_t)data);
                if (ret != DOLOS_SUCCESS) {
                        printf("Failed to write data from EEPROM, code: %d\r\n", ret);
                        return CLI_E_IO;
                }
                printf("Wrote EEPROM data at address %d = %02X\r\n", address, data);
        } else {
                printf("Usage: eeprom-write <address> <data>\r\n");
        }
        return CLI_OK;
}

static cli_status_t cmd_get_serial_no_func(int argc, char **argv)
{
        char serial[EEPROM_SERIAL_SIZE + 1] = { 0 };
        int ret;

        ret = eeprom_serial_read(serial);
        if (ret != DOLOS_SUCCESS) {
                printf("Failed to read serial from EEPROM, code: %d\r\n", ret);
                return CLI_E_IO;
        }

        printf("%s\r\n", serial);

        return CLI_OK;
}

static cli_status_t cmd_set_serial_no_func(int argc, char **argv)
{
        if (argc == 2) {
                int ret;

                if (!eeprom_serial_validate(argv[1])) {
                        printf("Invalid serial, format is: %s\r\n", EEPROM_SERIAL_FORMAT);
                        return CLI_E_INVALID_ARGS;
                }

                ret = eeprom_serial_write(argv[1]);
                if (ret != DOLOS_SUCCESS) {
                        printf("Failed to write serial to EEPROM, code: %d\r\n", ret);
                        return CLI_E_IO;
                }
                printf("Wrote serial %s to EEPROM\r\n", argv[1]);
        } else {
                printf("Usage: serial-write %s\r\n", EEPROM_SERIAL_FORMAT);
                return CLI_E_INVALID_ARGS;
        }
        return CLI_OK;
}

static cli_status_t cmd_ver_func(int argc, char **argv)
{
        printf("version 0.1\n\r");

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
        { .cmd = "output", .help = "Force turnon the output.", .func = cmd_output_func },
        { .cmd = "sb-print", .help = "Prints smart battery register data", .func = cmd_sb_print_func },
        { .cmd = "echo", .help = "Enables CLI echo (echo is disabled by default)", .func = cmd_cli_echo_func },
        { .cmd = "jump-to-bsl", .help = "Invokes BSL for Dolos updates", .func = cmd_bsl_invoke_func },
        { .cmd = "performance", .help = "Prints performance for IRQ modules", .func = cmd_perf_func },
        { .cmd = "reset-smbus", .help = "Resets Dolos target SMBus", .func = cmd_reset_smbus_func },
        { .cmd = "eeprom-read", .help = "Reads data from connector EEPROM", .func = cmd_eeprom_read_func },
        { .cmd = "eeprom-write", .help = "Writes data to connector EEPROM", .func = cmd_eeprom_write_func },
        { .cmd = "get-serial-no", .help = "Reads serial from connector EEPROM", .func = cmd_get_serial_no_func },
        { .cmd = "set-serial-no", .help = "Writes serial to connector EEPROM", .func = cmd_set_serial_no_func },
        { .cmd = "version", .help = "Prints version", .func = cmd_ver_func },
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
