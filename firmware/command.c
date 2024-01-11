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
#include "dolos_timers.h"
#if __has_include("version.h")
#include "version.h"
#else
#define DOLOS_VERSION "Invalid Build"
#endif

#define CMD_SMBUS_HEADER_USAGE "Usage (All data except for <num_of_bytes> should be in hex format):\r\n"
#define CMD_SMBUS_WRITE_BYTE_USAGE "   smbus <7_bit_target_address> w b <cmd> <byte>\r\n"
#define CMD_SMBUS_WRITE_WORD_USAGE "   smbus <7_bit_target_address> w w <cmd> <first_byte> <second_byte>\r\n"
#define CMD_SMBUS_WRITE_BLOCK_USAGE "   smbus <7_bit_target_address> w B <cmd> <num_of_bytes> <n_bytes...>\r\n"
#define CMD_SMBUS_READ_USAGE "   smbus <7_bit_target_address> r {b|w|B} <cmd>\r\n"
#define CMD_SMBUS_READ_BYTE_USAGE "   smbus <7_bit_target_address> r b <cmd>\r\n"
#define CMD_SMBUS_READ_WORD_USAGE "   smbus <7_bit_target_address> r w <cmd>\r\n"
#define CMD_SMBUS_READ_BLOCK_USAGE "   smbus <7_bit_target_address> r B <cmd>\r\n"

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
        printf("version %s\n\r", DOLOS_VERSION);

        return CLI_OK;
}

static cli_status_t cmd_smbus_write_byte_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_WRITE_BYTE_USAGE;

        if (argc < 6) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int cmd;
        int parsed_byte;
        uint8_t byte;

        if (parse_hex(argv[4], &cmd) != PARSE_SUCCESS || cmd < 0 || cmd >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (parse_hex(argv[5], &parsed_byte) != PARSE_SUCCESS || parsed_byte < 0 || parsed_byte >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int ret;

        byte = parsed_byte;

        ret = dsb_controller_write(target_address, cmd, &byte, 1);

        if (ret != DOLOS_SUCCESS) {
                printf("Failed to write to target=0x%02X address=0x%02X byte=0x%02x, err_code=%d\r\n", target_address,
                       cmd, byte, ret);
                return CLI_E_IO;
        }

        printf("Wrote to target=0x%02X, address=0x%02X, byte=0x%02x\r\n", target_address, cmd, byte);
        return CLI_OK;
}

static cli_status_t cmd_smbus_write_word_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_WRITE_WORD_USAGE;

        if (argc < 7) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int cmd;
        int parsed_byte[2];
        uint8_t word[2];

        if (parse_hex(argv[4], &cmd) != PARSE_SUCCESS || cmd < 0 || cmd >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (parse_hex(argv[5], &parsed_byte[0]) != PARSE_SUCCESS || parsed_byte[0] < 0 || parsed_byte[0] >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (parse_hex(argv[6], &parsed_byte[1]) != PARSE_SUCCESS || parsed_byte[1] < 0 || parsed_byte[1] >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int ret;

        word[0] = (uint8_t)parsed_byte[0];
        word[1] = (uint8_t)parsed_byte[1];
        ret = dsb_controller_write(target_address, cmd, &word[0], 2);

        if (ret != DOLOS_SUCCESS) {
                printf("Failed to write to target=0x%02X address=0x%02X word=0x%04x, err_code=%d\r\n", target_address,
                       cmd, *((uint16_t *)&word[0]), ret);
                return CLI_E_IO;
        }

        printf("Wrote to target=0x%02X, address=0x%02X, word=0x%04x\r\n", target_address, cmd, *((uint16_t *)&word[0]));
        return CLI_OK;
}

static cli_status_t cmd_smbus_write_block_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_WRITE_BLOCK_USAGE;

        if (argc < 6) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int cmd;
        int parsed_byte;
        size_t length;
        uint8_t block[SMB_MAX_PACKET_SIZE];

        if (parse_hex(argv[4], &cmd) != PARSE_SUCCESS || cmd < 0 || cmd >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (parse_int(argv[5], (int *)&length) != PARSE_SUCCESS || length < 0 || length > SMB_MAX_PACKET_SIZE) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (argc < 5 + length) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        for (size_t i = 0; i < length; i++) {
                if (parse_hex(argv[i + 6], &parsed_byte) != PARSE_SUCCESS || parsed_byte < 0 || parsed_byte >= 256) {
                        printf("%s", cmd_usage);
                        return CLI_E_INVALID_ARGS;
                }
                block[i] = (uint8_t)parsed_byte;
        }

        int ret;

        ret = dsb_controller_write(target_address, cmd, &block[0], length);

        if (ret != DOLOS_SUCCESS) {
                printf("Failed to write block of data to target=0x%02X address=0x%02X, err_code=%d\r\n", target_address,
                       cmd, ret);
                return CLI_E_IO;
        }

        printf("Wrote block of data to target=0x%02X, address=0x%02X\r\n", target_address, cmd);

        return CLI_OK;
}

static cli_status_t cmd_smbus_write_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] =
                CMD_SMBUS_HEADER_USAGE CMD_SMBUS_WRITE_BYTE_USAGE CMD_SMBUS_WRITE_WORD_USAGE CMD_SMBUS_WRITE_BLOCK_USAGE;

        if (argc < 4) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (!strcmp(argv[3], "b")) {
                /* Handle SMBus write byte command */
                return cmd_smbus_write_byte_func(argc, argv, target_address);
        } else if (!strcmp(argv[3], "w")) {
                /* Handle SMBus write word command */
                return cmd_smbus_write_word_func(argc, argv, target_address);
        } else if (!strcmp(argv[3], "B")) {
                /* Handle SMBus write block command */
                return cmd_smbus_write_block_func(argc, argv, target_address);
        } else {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }
}

static cli_status_t cmd_smbus_read_byte_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_READ_BYTE_USAGE;

        if (argc < 5) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int cmd;

        if (parse_hex(argv[4], &cmd) != PARSE_SUCCESS || cmd < 0 || cmd >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int ret;
        uint8_t byte;

        ret = dsb_controller_read_byte(target_address, cmd, &byte);

        if (ret != DOLOS_SUCCESS) {
                printf("Failed to read from target=0x%02X address=0x%02X, err_code=%d\r\n", target_address, cmd, ret);
                return CLI_E_IO;
        }

        printf("Read from target=0x%02X address=0x%02X byte=0x%02x\r\n", target_address, cmd, byte);

        return CLI_OK;
}

static cli_status_t cmd_smbus_read_word_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_READ_WORD_USAGE;

        if (argc < 5) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int cmd;

        if (parse_hex(argv[4], &cmd) != PARSE_SUCCESS || cmd < 0 || cmd >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int ret;
        uint8_t word[2];

        ret = dsb_controller_read_word(target_address, cmd, &word[0]);

        if (ret != DOLOS_SUCCESS) {
                printf("Failed to read from target=0x%02X address=0x%02X, err_code=%d\r\n", target_address, cmd, ret);
                return CLI_E_IO;
        }

        printf("Read from target=0x%02X address=0x%02X word=0x%04x\r\n", target_address, cmd, *((uint16_t *)&word[0]));

        return CLI_OK;
}

static cli_status_t cmd_smbus_read_block_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_READ_BLOCK_USAGE;

        if (argc < 5) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int cmd;

        if (parse_hex(argv[4], &cmd) != PARSE_SUCCESS || cmd < 0 || cmd >= 256) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int ret;
        uint8_t block[SMB_MAX_PACKET_SIZE];
        size_t length = 0;

        ret = dsb_controller_read_block(target_address, cmd, &block[0], &length);

        if (ret != DOLOS_SUCCESS) {
                printf("Failed to read from target=0x%02X address=0x%02X, err_code=%d\r\n", target_address, cmd, ret);
                return CLI_E_IO;
        }

        printf("Read from target=0x%02X address=0x%02X length=%d\r\n", target_address, cmd, length);
        for (size_t i = 0; i < length; i++) {
                printf("%02X ", block[i]);
                if ((i & 0xf) == 0) {
                        printf("\r\n");
                }
        }

        return CLI_OK;
}

static cli_status_t cmd_smbus_read_func(int argc, char **argv, uint8_t target_address)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_READ_USAGE;

        if (argc < 4) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (!strcmp(argv[3], "b")) {
                /* Handle SMBus read byte command */
                return cmd_smbus_read_byte_func(argc, argv, target_address);
        } else if (!strcmp(argv[3], "w")) {
                /* Handle SMBus read word command */
                return cmd_smbus_read_word_func(argc, argv, target_address);
        } else if (!strcmp(argv[3], "B")) {
                /* Handle SMBus read block command */
                return cmd_smbus_read_block_func(argc, argv, target_address);
        } else {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }
}

static cli_status_t cmd_smbus_func(int argc, char **argv)
{
        const char cmd_usage[] = CMD_SMBUS_HEADER_USAGE CMD_SMBUS_WRITE_BYTE_USAGE CMD_SMBUS_WRITE_WORD_USAGE
                CMD_SMBUS_WRITE_BLOCK_USAGE CMD_SMBUS_READ_USAGE;
        if (argc < 3) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        int target_address;

        if (parse_hex(argv[1], &target_address) != PARSE_SUCCESS || target_address < 0 || target_address >= 128) {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }

        if (!strcmp(argv[2], "w")) {
                /* Handle SMBus write command */
                return cmd_smbus_write_func(argc, argv, (uint8_t)target_address);
        } else if (!strcmp(argv[2], "r")) {
                /* Handle SMBus read command */
                return cmd_smbus_read_func(argc, argv, (uint8_t)target_address);
        } else {
                printf("%s", cmd_usage);
                return CLI_E_INVALID_ARGS;
        }
}

static cli_status_t cmd_gpio_func(int argc, char **argv)
{
        const char usage[] = "Provide a valid pin name: efuse-pg, chrg-ok\r\n";
        if (argc != 2) {
                printf("%s", usage);
                return CLI_E_INVALID_ARGS;
        }

        bool state;

        if (!strcmp(argv[1], "efuse-pg")) {
                state = DL_GPIO_readPins(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);
                printf("EFSUE_PG pin state: %d\r\n", state);
        } else if (!strcmp(argv[1], "chrg-ok")) {
                state = DL_GPIO_readPins(BQ25731_GROUP_CHRG_OK_PORT, BQ25731_GROUP_CHRG_OK_PIN);
                printf("CHRG_OK pin state: %d\r\n", state);
        } else {
                printf("%s", usage);
                return CLI_E_INVALID_ARGS;
        }
        return CLI_OK;
}

static cli_status_t cmd_status_func(int argc, char **argv)
{
        printf("Dolos Status:\r\n");

        /* Print Up-time */
        uint8_t seconds;
        uint8_t minutes;
        uint8_t hours;
        uint32_t days;

        dtimers_get_uptime(&seconds, &minutes, &hours, &days);
        printf("   Uptime: %d Days and %02d:%02d:%02d seconds\r\n", days, hours, minutes, seconds);

        /* Print charger if it was detected (BQ CHRG_OK signal)*/
        printf("   Charger: %s\r\n", dgpio_get_bq_charge_ok_signal() ? "Detected" : "Not Detected");

        /* Print system present polarity and if system is present */
        bool is_present = dgpio_get_system_present_signal();
        bool polarity = dconfig_get_system_present_polarity();
        bool signal = !(polarity != is_present);
        printf("   System present: %s (Polarity: %s) (Signal: %s)\r\n", (is_present ? "Present" : "Not Present"),
               (polarity ? "High" : "Low"), (signal ? "High" : "Low"));

        /* Prints SMBus communication state */
        printf("   SMBus: Communication %s\r\n", (dsb_target_get_communication_state() ? "detected" : "not detected"));

        /* Prints the PAC values: */
        printf("   PAC: %dmV %dmA\r\n", pac_read_voltage_mv(), pac_read_current_ma());

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
        { .cmd = "smbus", .help = "Sends commands through the SMBus controller on Dolos", .func = cmd_smbus_func },
        { .cmd = "gpio", .help = "Checks the state of the GPIO pins on Dolos", .func = cmd_gpio_func },
        { .cmd = "status", .help = "Shows general Dolos status", .func = cmd_status_func },
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
