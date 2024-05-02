/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"
#include "smart_battery.h"
#include "smbus_target.h"

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define LED_PRG_NODE DT_NODELABEL(ledprg)
#define LED_ERR_NODE DT_NODELABEL(lederr)
#define LED_PRG DT_NODE_CHILD_IDX(LED_PRG_NODE)
#define LED_ERR DT_NODE_CHILD_IDX(LED_ERR_NODE)

static const struct device *leds = DEVICE_DT_GET(DT_PARENT(LED_PRG_NODE));

int main(void)
{
	printk("Starting Dolos\n");

	led_on(leds, LED_PRG);

	if (eeprom_read_data() != 0) {
		led_on(leds, LED_ERR);
	}

	if (eeprom_crc32_check() != 0) {
		led_on(leds, LED_ERR);
	}

	sb_init();
	smbus_target_init();

	while (true) {
		k_sleep(K_FOREVER);
	}
}
