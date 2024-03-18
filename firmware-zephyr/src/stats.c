/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bms.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(stats, LOG_LEVEL_DBG);
const struct device *i2c_controller_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
const struct device *i2c_target_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

void print_stats(const struct shell *sh, const struct device *i2c_dev)
{
	if (i2c_dev != NULL) {
		struct i2c_device_state *state = CONTAINER_OF(
			i2c_dev->state, struct i2c_device_state, devstate);
		if (state != NULL) {
			shell_print(sh, "   Messages Count     : %u",
				    state->stats.message_count);
			shell_print(sh, "   Transfer Call Count: %u",
				    state->stats.transfer_call_count);
			shell_print(sh, "   Bytes Read         : %u",
				    state->stats.bytes_read);
			shell_print(sh, "   Bytes Written      : %u",
				    state->stats.bytes_written);
		}
	} else {
		LOG_ERR("Failed to find i2c device.");
	}
}

static int print_i2c_target_stats(const struct shell *sh, size_t argc,
				  char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	print_stats(sh, i2c_target_dev);

	return 0;
}

static int print_i2c_controller_stats(const struct shell *sh, size_t argc,
				      char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	print_stats(sh, i2c_controller_dev);

	return 0;
}

static int print_i2c_stats(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Controller statistics:");
	print_stats(sh, i2c_controller_dev);

	shell_print(sh, "\nTarget statistics:");
	print_stats(sh, i2c_target_dev);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_stats_i2c,
	SHELL_CMD(target, NULL, "Displays i2c statistics for target.",
		  print_i2c_target_stats),
	SHELL_CMD(controller, NULL, "Displays i2c statistics for controller.",
		  print_i2c_controller_stats),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_stats,
	SHELL_CMD(gpio, &sub_stats_i2c, "Displays GPIO statistics.",
		  print_gpio_stats_handler),
	SHELL_CMD(i2c, &sub_stats_i2c,
		  "Displays i2c statistics for target and controller.",
		  print_i2c_stats),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(stats, &sub_stats, "Statistics command.", NULL);
