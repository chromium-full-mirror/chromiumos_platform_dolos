/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

static const struct device *leds = DEVICE_DT_GET(DT_NODELABEL(leds));
#define LED_RDY DT_NODE_CHILD_IDX(DT_NODELABEL(ledrdy))
#define LED_PWR DT_NODE_CHILD_IDX(DT_NODELABEL(ledpwr))

LOG_MODULE_REGISTER(bms, LOG_LEVEL_DBG);

/**
 * GPIO info
 */
static const struct gpio_dt_spec sys_pres_spec =
	GPIO_DT_SPEC_GET(DT_NODELABEL(system_present), gpios);
static const struct gpio_dt_spec out_en_spec =
	GPIO_DT_SPEC_GET(DT_NODELABEL(output_enable), gpios);
static const struct gpio_dt_spec efuse_pg_spec =
	GPIO_DT_SPEC_GET(DT_NODELABEL(efuse_pg), gpios);
static const struct gpio_dt_spec bq_chrg_ok =
	GPIO_DT_SPEC_GET(DT_NODELABEL(bq_charge_ok), gpios);

static bool sys_pres_pol = true;

/**
 * Gets the system present signal and takes polarity into condsideration
 */
static bool gpio_get_sys_pres(void)
{
	bool sys_pres_sig = gpio_pin_get_dt(&sys_pres_spec);
	return (sys_pres_pol == sys_pres_sig);
}

/**
 * @brief Enables Dolos output.
 */
static void bms_enable_power_output(bool enable)
{
	if (enable) {
		gpio_pin_set_dt(&out_en_spec, 1);
		led_on(leds, LED_PWR);
	} else {
		gpio_pin_set_dt(&out_en_spec, 0);
		led_off(leds, LED_PWR);
	}
}

/**
 * Helper struct to initialize GPIO
 */
struct gpio_config {
	const struct gpio_dt_spec *pin_spec;
	uint32_t dir;
};

static struct gpio_config gpio_configs[] = {
	{ &sys_pres_spec, GPIO_INPUT },
	{ &efuse_pg_spec, GPIO_INPUT },
	{ &bq_chrg_ok, GPIO_INPUT },
	{ &out_en_spec, GPIO_OUTPUT_INACTIVE },
};

/**
 * Event info
 */
K_EVENT_DEFINE(bms_event_data);

enum bms_event {
	BMS_EVENT_SYSTEM_PRESENT = BIT(0),
	BMS_EVENT_EFUSE_PG = BIT(1),
	BMS_EVENT_BQ_CHRG_OK = BIT(2),
	BMS_EVENT_TIMER = BIT(3),
	BMS_EVENT_STATE_CHANGE = BIT(4),
};

#define BMS_EVENT_ALL                                                          \
	BMS_EVENT_SYSTEM_PRESENT | BMS_EVENT_EFUSE_PG | BMS_EVENT_BQ_CHRG_OK | \
		BMS_EVENT_TIMER | BMS_EVENT_STATE_CHANGE

/**
 * BMS info
 */
struct bms_signal_state_tracker {
	/* Current state of the pin - true = HIGH, false = LOW */
	bool state;
	/* No of ticks passed since the low/high state. */
	uint8_t ticks[2];
	/* No of ticks needed to switch to that state. */
	uint8_t min_ticks[2];
};

static struct bms_signal_state_tracker efuse_signal_state = {
	.state = false,
	.ticks = { 0 },
	.min_ticks = { 2, 2 },
};

static struct bms_signal_state_tracker system_present_signal_state = {
	.state = false,
	.ticks = { 0 },
	.min_ticks = { 2, 2 },

};

static struct bms_signal_state_tracker bq_charge_ok_signal_state = {
	.state = false,
	.ticks = { 0 },
	.min_ticks = { 1, 1 },
};

/**
 * EFUSE_PG Timer
 */
struct k_timer efuse_pg_timer;

K_TIMER_DEFINE(efuse_pg_timer, NULL, NULL);

/**
 * State machine info
 */
enum bms_state {
	BMS_STATE_POWER_OUTPUT_OFF = 0,
	BMS_STATE_TIMER,
	BMS_STATE_POWER_GOOD_WAIT,
	BMS_STATE_POWER_OUTPUT_ON,
};

#define EXPAND_ENUM_NAME(x) #x
#define ENUM_NAME(x) EXPAND_ENUM_NAME(x)

struct bms_state_data {
	struct smf_ctx ctx;
} state_data;

static const struct smf_state bms_states[];

/**
 * @brief BMS_STATE_POWER_OUTPUT_OFF run handler
 */
static void power_output_off_state_run(void *o)
{
	bms_enable_power_output(false);

	if (system_present_signal_state.state &&
	    bq_charge_ok_signal_state.state) {
		LOG_DBG("Changing state to %s", ENUM_NAME(BMS_STATE_TIMER));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_TIMER]);
	}
}

/**
 * @brief BMS_STATE_TIMER entry handler
 */
static void timer_state_entry(void *o)
{
	LOG_DBG("Executing %s entry", ENUM_NAME(BMS_STATE_TIMER));

	bms_enable_power_output(true);
	k_timer_start(&efuse_pg_timer, K_SECONDS(5), K_NO_WAIT);
}

/**
 * @brief BMS_STATE_TIMER run handler
 */
static void timer_state_run(void *o)
{
	if (!system_present_signal_state.state ||
	    !bq_charge_ok_signal_state.state) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_OUTPUT_OFF]);
		return;
	}

	if (k_timer_status_get(&efuse_pg_timer) == 0) {
		return;
	}

	if (efuse_signal_state.state) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_ON));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_OUTPUT_ON]);
	} else {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_GOOD_WAIT));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_GOOD_WAIT]);
	}
}

/**
 * @brief BMS_STATE_POWER_GOOD_WAIT run handler
 */
static void power_good_wait_state_run(void *o)
{
	bms_enable_power_output(false);

	if (!system_present_signal_state.state ||
	    !bq_charge_ok_signal_state.state) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_OUTPUT_OFF]);
		return;
	}

	if (efuse_signal_state.state) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_ON));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_OUTPUT_ON]);
	}
}

/**
 * @brief BMS_STATE_POWER_OUTPUT_ON run handler
 */
static void power_output_on_state_run(void *o)
{
	bms_enable_power_output(true);

	if (!system_present_signal_state.state ||
	    !bq_charge_ok_signal_state.state) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_OUTPUT_OFF]);
		return;
	}

	if (!efuse_signal_state.state) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_GOOD_WAIT));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_GOOD_WAIT]);
	}
}

static const struct smf_state bms_states[] = {
	[BMS_STATE_POWER_OUTPUT_OFF] =
		SMF_CREATE_STATE(NULL, power_output_off_state_run, NULL),
	[BMS_STATE_TIMER] =
		SMF_CREATE_STATE(timer_state_entry, timer_state_run, NULL),
	[BMS_STATE_POWER_GOOD_WAIT] =
		SMF_CREATE_STATE(NULL, power_good_wait_state_run, NULL),
	[BMS_STATE_POWER_OUTPUT_ON] =
		SMF_CREATE_STATE(NULL, power_output_on_state_run, NULL),
};

/**
 * State machine thread info
 */
#define BMS_STATE_MACHINE_THREAD_STACK_SIZE 512
#define BMS_STATE_MACHINE_THREAD_PRIORITY 0

/**
 * @brief BMS state machine task function. Runs the current state handler
 * continously.
 */
void bms_state_machine_task_fn(void *, void *, void *);

K_THREAD_STACK_DEFINE(bms_state_machine_thread_stack_area,
		      BMS_STATE_MACHINE_THREAD_STACK_SIZE);
struct k_thread bms_state_machine_thread_data;

/**
 * BMS tick handler thread info
 */
#define BMS_TICK_HANDLER_THREAD_STACK_SIZE 512
#define BMS_TICK_HANDLER_THREAD_PRIORITY 0

/**
 * @brief BMS tick handler task function. Monitors the change of state for each
 * GPIO control pin every 1 second.
 */
void bms_tick_handler_task_fn(void *, void *, void *);

K_THREAD_DEFINE(bms_tick_handler_thread, BMS_TICK_HANDLER_THREAD_STACK_SIZE,
		bms_tick_handler_task_fn, NULL, NULL, NULL,
		BMS_TICK_HANDLER_THREAD_PRIORITY, 0, 0);

/**
 * @brief Proccesses signal state change.
 *
 * @return Returns true if state changed, false otherwise.
 */
static bool bms_process_state_change(struct bms_signal_state_tracker *tracker,
				     bool new_state)
{
	if (tracker->ticks[new_state] < tracker->min_ticks[new_state]) {
		tracker->ticks[new_state]++;
	} else if (tracker->state != new_state) {
		tracker->state = new_state;
		tracker->ticks[!new_state] = 0;
		return true;
	}
	return false;
}

/**
 * @brief Initializes the BMS GPIO control pins
 *
 * @retval 0 on success
 * @retval Negative on failure
 */
static int bms_init(void)
{
	int ret;

	LOG_DBG("Initializing GPIO pins");

	for (size_t i = 0; i < ARRAY_SIZE(gpio_configs); i++) {
		struct gpio_config *pin_config = &gpio_configs[i];

		if (!gpio_is_ready_dt(pin_config->pin_spec)) {
			LOG_ERR("GPIO Pin %s is not ready",
				pin_config->pin_spec->port->name);
			return -ENOTSUP;
		}

		ret = gpio_pin_configure_dt(pin_config->pin_spec,
					    pin_config->dir);
		if (ret != 0) {
			LOG_ERR("Failed to configure input pin %s, err=%d",
				pin_config->pin_spec->port->name, ret);
			return ret;
		}
	}

	LOG_DBG("Initialized all GPIO bins successfully");

	LOG_DBG("Getting System present polarity");

	uint8_t eeprom_sys_pres_pol;
	ret = eeprom_get_sys_pres_pol(&eeprom_sys_pres_pol);

	if (ret != 0) {
		LOG_ERR("Failed to configure SYSTEM_PRESENT pin polarity");
		return ret;
	}

	sys_pres_pol = (bool)eeprom_sys_pres_pol;

	return 0;
}

/**
 * @brief Check and change the state if the GPIO control pin signal is stable
 * for at least a certain number of ticks.
 */
void bms_tick_handler(void)
{
	/* Check and change SW states if HW signal is stable for at least given
	 * number of seconds. */
	if (bms_process_state_change(&system_present_signal_state,
				     gpio_get_sys_pres())) {
		LOG_DBG("Detected a change in SYSTEM_PRESENT, signal=%d",
			system_present_signal_state.state);
	}

	if (bms_process_state_change(&bq_charge_ok_signal_state,
				     gpio_pin_get_dt(&bq_chrg_ok))) {
		LOG_DBG("Detected a change in BQ_CHRG_OK, signal=%d",
			bq_charge_ok_signal_state.state);
	}

	if (bms_process_state_change(&efuse_signal_state,
				     gpio_pin_get_dt(&efuse_pg_spec))) {
		LOG_DBG("Detected a change in EFUSE_PG, signal=%d",
			efuse_signal_state.state);
	}

	/* Dolos is considered ready when BB CHARGE OK signal is asserted. */
	if (bq_charge_ok_signal_state.state) {
		led_on(leds, LED_RDY);
	} else {
		led_off(leds, LED_RDY);
	}
}

void bms_state_machine_task_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;

	smf_set_initial(SMF_CTX(&state_data),
			&bms_states[BMS_STATE_POWER_OUTPUT_OFF]);

	while (true) {
		ret = smf_run_state(SMF_CTX(&state_data));

		if (ret != 0) {
			LOG_ERR("State machine failed to execute, err=%d", ret);
			smf_set_terminate(SMF_CTX(&state_data), ret);
		}

		k_sleep(K_MSEC(50));
	}
}

void bms_tick_handler_task_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;

	ret = bms_init();

	if (ret != 0) {
		LOG_ERR("Failed to start BMS tick handler task, err=%d", ret);
		return;
	}

	k_thread_create(
		&bms_state_machine_thread_data,
		bms_state_machine_thread_stack_area,
		K_THREAD_STACK_SIZEOF(bms_state_machine_thread_stack_area),
		bms_state_machine_task_fn, NULL, NULL, NULL,
		BMS_STATE_MACHINE_THREAD_PRIORITY, 0, K_NO_WAIT);

	while (true) {
		bms_tick_handler();
		k_sleep(K_SECONDS(1));
	}
}
