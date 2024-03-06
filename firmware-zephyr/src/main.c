/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "buck_boost.h"
#include "smart_battery.h"
#include "smbus_target.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
	printk("Starting Dolos\n");

	sb_reg_load();
	smbus_target_init();

	bq25731_init();

	while (true) {
		k_sleep(K_FOREVER);
	}
}
