/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "buck_boost.h"
#include "smart_battery.h"
#include "smbus_target.h"

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define LED_PRG_NODE DT_NODELABEL(ledprg)
#define LED_PRG DT_NODE_CHILD_IDX(LED_PRG_NODE)

static const struct device *leds = DEVICE_DT_GET(DT_PARENT(LED_PRG_NODE));

int main(void)
{
	printk("Starting Dolos\n");

	led_on(leds, LED_PRG);

	sb_reg_load();
	smbus_target_init();

	bq25731_init();

	while (true) {
		k_sleep(K_FOREVER);
	}
}
