/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "app_version.h"
#include "commands.h"
#include "uart.h"
#include "utils.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>

typedef int (*command_handler_t)(void);

struct command_entry {
	const char *name;
	command_handler_t handler;
	const char *desc;
	// Flag to indicate if the command can be run even if CRC32 validation
	// fails
	const int always_safe;
};

static int command_boot(void);
static int command_bsl(void);
static int command_reset(void);
static int command_version(void);
static int command_help(void);

/*
 * Command dispatch table.
 */
static const struct command_entry command_table[] = {
	{ "reset", command_reset, "reboots the device", 1 },
	{ "bsl", command_bsl, "jumps to BSL", 1 },
	{ "boot", command_boot, "boots main image", 0 },
	{ "version", command_version, "prints bootloader version", 1 },
	{ "help", command_help, "prints help message", 1 },
	{ "boot_unsafe", command_boot,
	  "bypasses CRC32 validation and boots main image", 1 }
};

int process_command(char *buf, int safe_to_boot)
{
	for (int i = 0; i < ARRAY_SIZE(command_table); i++) {
		if (strcmp(buf, command_table[i].name) == 0) {
			if (command_table[i].always_safe == 0 &&
			    safe_to_boot == 0) {
				uart_print(
					"ERROR: Unsafe to execute this command in current state.\r\n");
				return -1;
			}
			return command_table[i].handler();
		}
	}

	uart_print("Unrecognized command\r\n");
	return 0;
}

/*
 * Command handler functions
 */

#define TABSTOP 8

static int command_help(void)
{
	uart_print("Available commands:\r\n");
	for (int i = 0; i < ARRAY_SIZE(command_table); i++) {
		uart_print(command_table[i].name);
		if (strlen(command_table[i].name) > TABSTOP) {
			uart_print("\t");
		} else {
			uart_print("\t\t");
		}
		uart_print("-- ");
		uart_print(command_table[i].desc);
		uart_print("\r\n");
	}
	return 0;
}

static int command_version(void)
{
	uart_print("Bootloader version ");
	uart_print(APP_VERSION_STRING);
	uart_print("\r\n");
	return 0;
}

static int command_reset(void)
{
	uart_print("Resetting device...\r\n");
	k_sleep(K_MSEC(100));
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

static int command_bsl(void)
{
	jump_to_bsl();
	return 0;
}

static int command_boot(void)
{
	return boot_main_image();
}
