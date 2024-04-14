/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "error.h"

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define LED_ERR_NODE DT_NODELABEL(lederr)
#define LED_ERR DT_NODE_CHILD_IDX(LED_ERR_NODE)

static const struct device *leds = DEVICE_DT_GET(DT_PARENT(LED_ERR_NODE));

#define X_DEF_STR(s) #s
#define DEF_STR(s) X_DEF_STR(s)

/* Circular buffer that contains the last MAX_ERRORS */
static struct error_info errors[MAX_ERRORS];
static size_t errors_producer;
static size_t curr_error_num;
char eeprom_status[32] = "Successful";

void error_log(enum error_source source, uint32_t line_number, int error_code)
{
	led_on(leds, LED_ERR);

	errors[errors_producer].timestamp = k_uptime_get();
	errors[errors_producer].source = source;
	errors[errors_producer].line_number = line_number;
	errors[errors_producer].error_code = error_code;

	++errors_producer;
	++curr_error_num;

	if (errors_producer == MAX_ERRORS) {
		errors_producer = 0;
	}
}

size_t error_list(struct error_info error_list[MAX_ERRORS])
{
	if (curr_error_num < MAX_ERRORS) {
		memcpy(error_list, errors,
		       sizeof(struct error_info) * curr_error_num);
	} else {
		memcpy(error_list, &errors[errors_producer],
		       sizeof(struct error_info) *
			       (MAX_ERRORS - errors_producer));
		memcpy(error_list + (MAX_ERRORS - errors_producer), &errors[0],
		       sizeof(struct error_info) * errors_producer);
	}

	return MIN(curr_error_num, MAX_ERRORS);
}

void error_clear(void)
{
	led_off(leds, LED_ERR);

	curr_error_num = 0;
	errors_producer = 0;
}

static char *error_source_to_str(enum error_source err_src)
{
	switch (err_src) {
	case ERROR_EEPROM:
		return "EEPROM";
	case ERROR_SMART_BATTERY:
		return "SMART_BATTERY";
	case ERROR_BMS:
		return "BMS";
	case ERROR_SMBUS_TARGET:
		return "SMBUS_TARGET";
	case ERROR_I2C:
		return "I2C";
	case ERROR_PAC:
		return "PAC";
	case ERROR_BUCKBOOST:
		return "BUCKBOOST";
	case ERROR_TEMPERATURE:
		return "TEMPERATURE";
	default:
		return "UNKOWN";
	}
}

static int error_clear_handler(const struct shell *sh, size_t argc, char **argv)
{
	error_clear();
	shell_print(sh, "Cleared all errors");

	return 0;
}

static int error_show_handler(const struct shell *sh, size_t argc, char **argv)
{
	struct error_info err_list[MAX_ERRORS];
	size_t err_cnt = 0;

	err_cnt = error_list(err_list);

	if (err_cnt == 0) {
		shell_print(sh, "No errors encountered");
		return 0;
	}

	uint64_t time_remaining;
	uint32_t hours;
	uint32_t min;
	uint32_t sec;
	uint32_t msec;

	shell_print(sh, "Listing last %d errors", err_cnt);
	for (size_t i = 0; i < err_cnt; i++) {
		time_remaining = err_list[i].timestamp;
		hours = time_remaining / (1000 * 60 * 60);
		time_remaining %= 1000 * 60 * 60;
		min = time_remaining / (1000 * 60);
		time_remaining %= 1000 * 60;
		sec = time_remaining / 1000;
		time_remaining %= 1000;
		msec = time_remaining;

		shell_error(sh, "[%06d:%02d:%02d:%03d] %s: line: %d, err: %d",
			    hours, min, sec, msec,
			    error_source_to_str(err_list[i].source),
			    err_list[i].line_number, err_list[i].error_code);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	error_sub_cmd,
	SHELL_CMD(clear, NULL, "Clears all errors", error_clear_handler),
	SHELL_CMD(show, NULL, "Shows last " DEF_STR(MAX_ERRORS) " erros",
		  error_show_handler),
	SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(error, &error_sub_cmd, "Error command set", NULL);
