/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "error.h"
#include "pac.h"

#include <stdio.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

#define DT_DRV_COMPAT microship_pac1954

LOG_MODULE_REGISTER(pac1954, LOG_LEVEL_INF);

/* I2C spec */
static const struct i2c_dt_spec i2c_spec = I2C_DT_SPEC_INST_GET(0);

/* PAC register addresses */
#define PAC1954_REFRESH_REGISTER 0x00
#define PAC1954_VOLTAGE_REGISTER 0x0F
#define PAC1954_CURRENT_REGISTER 0x13

/* PAC conversion constants */
#define PAC1954_VOLTAGE_CONVERSION_CONST 0.488288701
#define PAC1954_CURRENT_CONVERSION_CONST 1.5259021896696423

/* Time needed between refresh and reading the voltage/current registers */
#define PAC1954_REFRESH_DELAY_MS 2

/* PAC Thread info */
#define PAC1954_THREAD_STACK_SIZE 512
#define PAC1954_THREAD_PRIORITY 0

static uint32_t voltage_mv, current_ma;

/**
 * @brief Sends a refresh request to the PAC
 */
static void pac1954_refresh(void)
{
	int ret;
	uint8_t refresh_reg = PAC1954_REFRESH_REGISTER;

	LOG_DBG("Sending a refresh request to PAC");

	ret = i2c_write_dt(&i2c_spec, &refresh_reg, sizeof(refresh_reg));

	if (ret) {
		DOLOS_LOG_ERR(ERROR_PAC, ret,
			      "Failed to refresh PAC registers %d", ret);
		return;
	}

	k_sleep(K_MSEC(PAC1954_REFRESH_DELAY_MS));
}

/**
 * @brief Reads the voltage register from the PAC
 */
static void pac1954_read_voltage(void)
{
	int ret;
	uint8_t voltage_reg = PAC1954_VOLTAGE_REGISTER;
	uint8_t voltage_buf[2];

	LOG_DBG("Sending a voltage read request to PAC");

	ret = i2c_write_read_dt(&i2c_spec, &voltage_reg, sizeof(voltage_reg),
				voltage_buf, sizeof(voltage_buf));
	if (ret) {
		DOLOS_LOG_ERR(ERROR_PAC, ret,
			      "Failed to read voltage from PAC %d", ret);
		return;
	}

	voltage_mv =
		sys_get_be16(voltage_buf) * PAC1954_VOLTAGE_CONVERSION_CONST;
	LOG_DBG("Voltage read, voltage_mv=%d", (int)voltage_mv);
}

/**
 * @brief Reads the current register from the PAC
 */
static void pac1954_read_current(void)
{
	int ret;
	uint8_t current_reg = PAC1954_CURRENT_REGISTER;
	uint8_t current_buf[2];

	LOG_DBG("Sending a current read request to PAC");

	ret = i2c_write_read_dt(&i2c_spec, &current_reg, sizeof(current_reg),
				current_buf, sizeof(current_buf));
	if (ret) {
		DOLOS_LOG_ERR(ERROR_PAC, ret,
			      "Failed to read current from PAC %d", ret);
		return;
	}

	current_ma =
		sys_get_be16(current_buf) * PAC1954_CURRENT_CONVERSION_CONST;
	LOG_DBG("Current read, current_ma=%d", (int)current_ma);
}

static void pac1954_read(void)
{
	LOG_DBG("Reading PAC registers");

	pac1954_refresh();
	pac1954_read_voltage();
	pac1954_read_current();

	LOG_DBG("Finished reading PAC registers");
}

uint32_t pac1954_get_voltage_mv(void)
{
	return voltage_mv;
}

uint32_t pac1954_get_current_ma(void)
{
	return current_ma;
}

static void pac1954_thread_fn(void *p1, void *p2, void *p3)
{
	while (true) {
		pac1954_read();
		k_sleep(K_SECONDS(1));
	}
}

/* Starting PAC thread */
K_THREAD_DEFINE(pac1954_thread, PAC1954_THREAD_STACK_SIZE, pac1954_thread_fn,
		NULL, NULL, NULL, PAC1954_THREAD_PRIORITY, 0, 0);

/**
 * Handles the pac command which prints voltage and current
 */
static int cmd_pac_read(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Voltage: %dmV", voltage_mv);
	shell_print(sh, "Current: %dmA", current_ma);

	return 0;
}

SHELL_CMD_REGISTER(pac, NULL, "Print PAC readings", cmd_pac_read);
