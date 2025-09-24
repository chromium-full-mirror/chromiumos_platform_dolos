/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "error.h"
#include "led.h"

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define X_DEF_STR(s) #s
#define DEF_STR(s) X_DEF_STR(s)

/* Circular buffer that contains the last MAX_ERRORS */
static struct error_info errors[MAX_ERRORS];
static size_t errors_producer;
static size_t curr_error_num;
char eeprom_status[32] = "Successful";

int compare_errors(const void *a, const void *b)
{
	const struct error_info *error_a = (const struct error_info *)a;
	const struct error_info *error_b = (const struct error_info *)b;
	return (int)(error_a->timestamp - error_b->timestamp);
}

void error_log(enum error_source source, uint32_t line_number, int error_code)
{
	dled_error_led_turn_on();

	/* If no existing error sharing same err_code, line_number and source
	 then add a new entry */
	errors[errors_producer].timestamp = k_uptime_get();
	errors[errors_producer].source = source;
	errors[errors_producer].line_number = line_number;
	errors[errors_producer].error_code = error_code;
	errors[errors_producer].occurrences = 1;
	++errors_producer;
	++curr_error_num;

	if (errors_producer == MAX_ERRORS) {
		errors_producer = 0;
	}
}

void error_clear(void)
{
	dled_error_led_turn_off();
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
	int err_cnt = MIN(curr_error_num, MAX_ERRORS);
	if (err_cnt == 0) {
		shell_print(sh, "No errors encountered");
		return 0;
	}

	qsort(errors, err_cnt, sizeof(struct error_info), compare_errors);
	uint64_t time_remaining;
	uint32_t hours;
	uint32_t min;
	uint32_t sec;
	uint32_t msec;

	shell_print(sh, "Listing last %d errors", err_cnt);
	for (size_t i = 0; i < err_cnt; i++) {
		time_remaining = errors[i].timestamp;
		hours = time_remaining / (1000 * 60 * 60);
		time_remaining %= 1000 * 60 * 60;
		min = time_remaining / (1000 * 60);
		time_remaining %= 1000 * 60;
		sec = time_remaining / 1000;
		time_remaining %= 1000;
		msec = time_remaining;

		shell_error(
			sh,
			"[%06d:%02d:%02d:%03d] %s: line: %d, err: %d, occurrences: %d",
			hours, min, sec, msec,
			error_source_to_str(errors[i].source),
			errors[i].line_number, errors[i].error_code,
			errors[i].occurrences);
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
