/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"
#include "led.h"
#include "smart_battery.h"
#include "smbus_target.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
	printk("Starting Dolos\n");

	dled_program_led_turn_on();

	// EEPROM checks
	if (eeprom_read_data() != 0) {
		// We can not read the EEPROM
		dled_error_led_turn_on();
	} else if (eeprom_crc32_check() != 0) {
		// EEPROM CRC is invalid
		dled_error_led_turn_on();
	}

	sb_init();
	smbus_target_init();

	while (true) {
		k_sleep(K_FOREVER);
	}
}
