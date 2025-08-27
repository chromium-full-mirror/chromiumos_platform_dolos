/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "commands.h"
#include "uart.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/storage/flash_map.h>
#include "app_version.h"
#include "utils.h"

typedef int (*command_handler_t)(void);

struct command_entry {
	const char *name;
	command_handler_t handler;
        const char *desc;
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
	{ "reset", command_reset, "reboots the device"},
	{ "bsl", command_bsl, "jumps to BSL"},
	{ "boot", command_boot, "boots main image"},
        { "version", command_version, "prints bootloader version"},
        { "help", command_help, "prints help message"}
};

int process_command(char *buf)
{
	for (int i = 0; i < ARRAY_SIZE(command_table); i++) {
		if (strcmp(buf, command_table[i].name) == 0) {
                        return command_table[i].handler();
		}
	}

	uart_print("Unrecognized command\r\n");
        return 0;
}

/*
 * Command handler functions
 */
static int command_help(void)
{
        uart_print("Available commands:\r\n");
        for (int i = 0; i < ARRAY_SIZE(command_table); i++) {
		uart_print(command_table[i].name);
                uart_print("\t-- ");
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
