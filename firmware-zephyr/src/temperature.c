/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "error.h"
#include "temperature.h"

#include <math.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <unistd.h>

LOG_MODULE_REGISTER(temperature, LOG_LEVEL_INF);

static const struct adc_dt_spec adc_spec =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

/* Temperature Thread info */
#define TEMP_THREAD_STACK_SIZE 512
#define TEMP_THREAD_PRIORITY 0

#define BIAS_VOLTAGE 3.3
#define ADC_RESOLUTION 4096
#define THRM_A0 -4.232811E+02
#define THRM_A1 4.728797E+02
#define THRM_A2 -1.988841E+02
#define THRM_A3 4.869521E+01
#define THRM_A4 -1.158754E+00

#define TEMPERATURE_C_TO_K(temp) (temp + 273.15)
#define TEMPERATURE_C_TO_F(temp) (temp * (9.0 / 5.0) + 32)

static uint16_t temp_adc_raw;
static double temp_c;
static double temp_k;
static double temp_f;
static uint16_t adc_buf;

/* Configure ADC sequence */
static struct adc_sequence seq = {
	.options = NULL,
	.buffer = &adc_buf,
	.buffer_size = sizeof(adc_buf),
};

/**
 * @brief Initializes the ADC peripheral used for temperature measurement
 *
 * @return 0 on success, negative on failure
 */
int temperature_init(void)
{
	int ret;

	LOG_DBG("Initializing temperature ADC");

	if (!adc_is_ready_dt(&adc_spec)) {
		DOLOS_LOG_ERR(ERROR_TEMPERATURE, -1, "ADC device %s not ready",
			      adc_spec.dev->name);
		return -ENOTSUP;
	}

	ret = adc_channel_setup_dt(&adc_spec);
	if (ret != 0) {
		DOLOS_LOG_ERR(ERROR_TEMPERATURE, ret,
			      "Failed to set up ADC channel, code=%d", ret);
		return ret;
	}

	ret = adc_sequence_init_dt(&adc_spec, &seq);
	if (ret != 0) {
		DOLOS_LOG_ERR(
			ERROR_TEMPERATURE, ret,
			"Failed to set up ADC conversion sequence, code=%d",
			ret);
		return ret;
	}

	LOG_DBG("Initialized temperature ADC");
	return 0;
}

/**
 * @brief Reads the temperature from the ADC
 */
void temperature_read_adc(void)
{
	LOG_DBG("Starting temperature read");

	/* Read the ADC value from the temperature sensor */
	int ret = adc_read_dt(&adc_spec, &seq);

	if (ret != 0) {
		DOLOS_LOG_ERR(ERROR_TEMPERATURE, ret,
			      "ERROR: ADC read failed, err=%d", ret);
		return;
	}

	/* Get the ADC raw value */
	uint16_t *adc_buf = (uint16_t *)seq.buffer;
	temp_adc_raw = adc_buf[0];

	/* Temperature calculations */
	double dtemp_voltage =
		(BIAS_VOLTAGE / ADC_RESOLUTION) * (double)temp_adc_raw;
	temp_c = (THRM_A4 * pow(dtemp_voltage, 4)) +
		 (THRM_A3 * pow(dtemp_voltage, 3)) +
		 (THRM_A2 * pow(dtemp_voltage, 2)) + (THRM_A1 * dtemp_voltage) +
		 THRM_A0;

	temp_k = TEMPERATURE_C_TO_K(temp_c);
	temp_f = TEMPERATURE_C_TO_F(temp_c);

	LOG_DBG("Temperature read, val=%f", temp_c);
}

double temperature_get_k(void)
{
	return temp_k;
}

double temperature_get_c(void)
{
	return temp_c;
}

double temperature_get_f(void)
{
	return temp_f;
}

static void temperature_thread_fn(void *p1, void *p2, void *p3)
{
	int ret;

	ret = temperature_init();
	if (ret != 0) {
		DOLOS_LOG_ERR(
			ERROR_TEMPERATURE, ret,
			"Could not initialize temperature sensor reading thread");
		return;
	}

	while (true) {
		temperature_read_adc();
		k_sleep(K_SECONDS(1));
	}
}

/* Starting temperature thread */
K_THREAD_DEFINE(temperature_thread, TEMP_THREAD_STACK_SIZE,
		temperature_thread_fn, NULL, NULL, NULL, TEMP_THREAD_PRIORITY,
		0, 0);

/**
 * Handles the temp command which prints tempreture in Celsius,Kelvin and
 * Fahrenheit
 */
static int cmd_temperature_read(const struct shell *sh, size_t argc,
				char **argv)
{
	shell_print(sh, "Temperature in Celsius (C)   : %7.2f", temp_c);
	shell_print(sh, "Temperature in Kelvin (K)    : %7.2f", temp_k);
	shell_print(sh, "Temperature in Fahrenheit (F): %7.2f", temp_f);
	shell_print(sh, "Raw ADC Temperature value    : %7d", temp_adc_raw);

	return 0;
}

SHELL_CMD_REGISTER(temp, NULL, "Print temperature readings",
		   cmd_temperature_read);
