/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "buck_boost.h"
#include "smart_battery.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
	printk("Starting Dolos\n");

	bq25731_init();

	sb_reg_load();

	while (true) {
		k_sleep(K_FOREVER);
	}
}
