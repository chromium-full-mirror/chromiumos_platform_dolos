/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "commands.h"
#include "uart.h"
#include "utils.h"

#include <zephyr/kernel.h>

#define MSG_SIZE 32
#define BOOT_TIMEOUT_SECONDS 10
#define BLINK_INTERVAL_SECONDS 1
#define PROMPT "\x1b[31mboot:~> \x1b[m"

K_TIMER_DEFINE(blink_timer, leds_callback, NULL);

int main(void)
{
	char cmd_buf[MSG_SIZE];
	int ret;

	uart_init();
	uart_print("\n*** Dolos bootloader startup ***\r\n");

	init_flags();

	k_timeout_t cmd_timeout = K_SECONDS(BOOT_TIMEOUT_SECONDS);
	uint8_t safe_to_boot = 1;

	if (check_firmware_CRC() != 0) {
		cmd_timeout = K_FOREVER;
		safe_to_boot = 0;
	} else if (check_dev_flag_set() == 0) {
		uart_print("Developer flag set -- skipping timeout\r\n");
		if (boot_main_image() != 0) {
			uart_print("Entering recovery mode...\r\n");
			uart_print("Waiting for user command\r\n");
			cmd_timeout = K_FOREVER;
		}
	}

	leds_init();
	k_timer_start(&blink_timer, K_SECONDS(BLINK_INTERVAL_SECONDS),
		      K_SECONDS(BLINK_INTERVAL_SECONDS));
	uart_print(PROMPT);
	while (1) {
		/* Wait for a command */
		ret = uart_receive_line(cmd_buf, sizeof(cmd_buf), cmd_timeout);

		if (ret == 0) {
			/* Command received from user */
			ret = process_command(cmd_buf, safe_to_boot);
			uart_print(PROMPT);
		} else if (ret == -EAGAIN) {
			/* Timeout occurred. Should not get here if validation
			   failed so it's safe to boot. */
			uart_print("\r\nTimeout reached. ");
			ret = boot_main_image();
		}

		if (ret != 0) {
			uart_print("Entering recovery mode...\r\n");
			uart_print("Waiting for user command\r\n");
			safe_to_boot = 0;
			cmd_timeout = K_FOREVER;
		}
	}
}
