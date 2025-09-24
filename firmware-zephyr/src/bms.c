/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bms.h"
#include "buck_boost.h"
#include "eeprom.h"
#include "error.h"
#include "led.h"
#include "smbus_target.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

static bool power_output_state = false;
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
 * There are certain devices that should not be powered on
 * only based on sys_present signal but also extra I2C communication
 */
static bool sys_pres_is_floating = false;
static bool force_sys_pres_flag;
static bool force_sys_pres_value;

/**
 * Gets the system present signal and takes polarity into condsideration
 */
static bool gpio_get_sys_pres(void)
{
	if (force_sys_pres_flag) {
		return force_sys_pres_value;
	}

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
		power_output_state = true;
		dled_powering_led_turn_on();
	} else {
		gpio_pin_set_dt(&out_en_spec, 0);
		power_output_state = false;
		dled_powering_led_turn_off();
	}
}

enum gpio_config_idx {
	GPIO_CONFIG_IDX_SYS_PRES = 0,
	GPIO_CONFIG_IDX_EFUSE_PG,
	GPIO_CONFIG_IDX_BQ_CHRG_OK,
	GPIO_CONFIG_IDX_OUT_EN,
};

bool bms_get_power_output_state()
{
	return power_output_state;
}

/**
 * Helper struct to initialize GPIO
 */
struct gpio_config {
	const struct gpio_dt_spec *pin_spec;
	uint32_t flags;
};

static struct gpio_config gpio_configs[] = {
	[GPIO_CONFIG_IDX_SYS_PRES] = { &sys_pres_spec, GPIO_INPUT },
	[GPIO_CONFIG_IDX_EFUSE_PG] = { &efuse_pg_spec, GPIO_INPUT },
	[GPIO_CONFIG_IDX_BQ_CHRG_OK] = { &bq_chrg_ok, GPIO_INPUT },
	[GPIO_CONFIG_IDX_OUT_EN] = { &out_en_spec, GPIO_OUTPUT_INACTIVE },
};

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
	/*stats for state changes*/
	uint32_t state_count;
};

static struct bms_signal_state_tracker efuse_signal_state = {
	.state = false,
	.ticks = { 0 },
	.min_ticks = { 2, 2 },
	.state_count = 0,
};

static struct bms_signal_state_tracker system_present_signal_state = {
	.state = false,
	.ticks = { 0 },
	.min_ticks = { 2, 2 },
	.state_count = 0,

};

static struct bms_signal_state_tracker bq_charge_ok_signal_state = {
	.state = false,
	.ticks = { 0 },
	.min_ticks = { 1, 1 },
	.state_count = 0,
};

/**
 * EFUSE_PG Timer
 */
struct k_timer efuse_pg_timer;

K_TIMER_DEFINE(efuse_pg_timer, NULL, NULL);

/**
 * State machine info
 */
struct bms_state_data {
	struct smf_ctx ctx;
} state_data;

static const struct smf_state bms_states[];

/**
 * @brief BMS_STATE_POWER_OUTPUT_OFF entry handler
 */
static void power_output_off_state_entry(void *o)
{
	LOG_DBG("Executing %s entry", ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
	bms_enable_power_output(false);
}

/**
 * @brief BMS_STATE_POWER_OUTPUT_OFF run handler
 */
static void power_output_off_state_run(void *o)
{
	if (system_present_signal_state.state &&
	    bq_charge_ok_signal_state.state) {
		LOG_DBG("Changing state to %s", ENUM_NAME(BMS_STATE_TIMER));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_TIMER]);
	}
}

static void handle_disconnect(void)
{
	if (sys_pres_is_floating) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_COMM_POWER_OFF));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_COMM_POWER_OFF]);
	} else {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_OUTPUT_OFF]);
	}
}

/**
 * @brief BMS_STATE_TIMER entry handler
 */
static void timer_state_entry(void *o)
{
	LOG_DBG("Executing %s entry", ENUM_NAME(BMS_STATE_TIMER));

	/* Make sure BQ is initialized correctly before enabling the output */
	bq25731_init();

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
		handle_disconnect();
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
 * @brief BMS_STATE_POWER_GOOD_WAIT entry handler
 */
static void power_good_wait_state_entry(void *o)
{
	LOG_DBG("Executing %s entry", ENUM_NAME(BMS_STATE_POWER_GOOD_WAIT));
	bms_enable_power_output(false);
}

/**
 * @brief BMS_STATE_POWER_GOOD_WAIT run handler
 */
static void power_good_wait_state_run(void *o)
{
	if (!system_present_signal_state.state ||
	    !bq_charge_ok_signal_state.state) {
		handle_disconnect();
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
 * @brief BMS_STATE_POWER_OUTPUT_ON entry handler
 */
static void power_output_on_state_entry(void *o)
{
	LOG_DBG("Executing %s entry", ENUM_NAME(BMS_STATE_POWER_OUTPUT_ON));
	bms_enable_power_output(true);
}

/**
 * @brief BMS_STATE_POWER_OUTPUT_ON run handler
 */
static void power_output_on_state_run(void *o)
{
	if (!system_present_signal_state.state ||
	    !bq_charge_ok_signal_state.state) {
		handle_disconnect();
		return;
	}

	if (!efuse_signal_state.state) {
		LOG_DBG("Changing state to %s",
			ENUM_NAME(BMS_STATE_POWER_GOOD_WAIT));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_POWER_GOOD_WAIT]);
	}
}

/**
 * @brief BMS_STATE_COMM_POWER_OFF entry handler
 */
static void comm_power_off_state_entry(void *o)
{
	LOG_DBG("Executing %s entry", ENUM_NAME(BMS_STATE_COMM_POWER_OFF));
	bms_enable_power_output(false);
}

/**
 * @brief BMS_STATE_COMM_POWER_OFF run handler
 */
static void comm_power_off_state_run(void *o)
{
	/*
	 * For devices with float polarity we should go out of power off state
	 * only when both sys_prest and smbus communication asserted.
	 * However we still want to persist forcing functionality working
	 * for devices with float polarity, as that is useful for debugging -
	 * this is why extra OR condition is applied.
	 * system_present_signal_state would be equal forced value when force
	 * flag on.
	 */
	if (system_present_signal_state.state &&
	    (smbus_target_is_comms_detected_short() || force_sys_pres_flag) &&
	    bq_charge_ok_signal_state.state) {
		LOG_DBG("Changing state to %s", ENUM_NAME(BMS_STATE_TIMER));
		smf_set_state(SMF_CTX(&state_data),
			      &bms_states[BMS_STATE_TIMER]);
	}
}

static const struct smf_state bms_states[] = {
	[BMS_STATE_POWER_OUTPUT_OFF] =
		SMF_CREATE_STATE(power_output_off_state_entry,
				 power_output_off_state_run, NULL, NULL, NULL),
	[BMS_STATE_TIMER] = SMF_CREATE_STATE(timer_state_entry, timer_state_run,
					     NULL, NULL, NULL),
	[BMS_STATE_POWER_GOOD_WAIT] =
		SMF_CREATE_STATE(power_good_wait_state_entry,
				 power_good_wait_state_run, NULL, NULL, NULL),
	[BMS_STATE_POWER_OUTPUT_ON] =
		SMF_CREATE_STATE(power_output_on_state_entry,
				 power_output_on_state_run, NULL, NULL, NULL),
	[BMS_STATE_COMM_POWER_OFF] =
		SMF_CREATE_STATE(comm_power_off_state_entry,
				 comm_power_off_state_run, NULL, NULL, NULL),
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
	uint8_t sys_pres_pol_eeprom;

	LOG_DBG("Getting System present polarity");
	sys_pres_pol_eeprom = eeprom_get_sys_pres_pol();

	switch (sys_pres_pol_eeprom) {
	case SYS_PRES_POL_LOW:
		sys_pres_is_floating = false;
		sys_pres_pol = false;
		gpio_configs[GPIO_CONFIG_IDX_SYS_PRES].flags |= GPIO_PULL_UP;
		break;
	case SYS_PRES_POL_FLOAT:
		sys_pres_is_floating = true;
		sys_pres_pol = true;
		gpio_configs[GPIO_CONFIG_IDX_SYS_PRES].flags |= GPIO_PULL_UP;
		break;
	case SYS_PRES_POL_HIGH:
		sys_pres_is_floating = false;
		sys_pres_pol = true;
		/* No pull up resistor needed for polarity HIGH */
		break;
	default:
		DOLOS_LOG_ERR(ERROR_BMS, -ENOTSUP,
			      "Not supported EEPROM SYS_POLARITY: %d",
			      sys_pres_pol_eeprom);
		return -ENOTSUP;
	}

	LOG_DBG("System present polarity obtained, sys_pres_pol=%d%s",
		sys_pres_pol,
		(sys_pres_pol ? "" : ", configured a pullup resistor"));

	LOG_DBG("Initializing GPIO pins");

	for (size_t i = 0; i < ARRAY_SIZE(gpio_configs); i++) {
		struct gpio_config *pin_config = &gpio_configs[i];

		if (!gpio_is_ready_dt(pin_config->pin_spec)) {
			DOLOS_LOG_ERR(ERROR_BMS, ret,
				      "GPIO Pin %s is not ready",
				      pin_config->pin_spec->port->name);
			return -ENOTSUP;
		}

		ret = gpio_pin_configure_dt(pin_config->pin_spec,
					    pin_config->flags);
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_BMS, ret,
				"Failed to configure input pin %s, err=%d",
				pin_config->pin_spec->port->name, ret);
			return ret;
		}
	}

	LOG_DBG("Initialized all GPIO bins successfully");

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
		system_present_signal_state.state_count++;
	}

	if (bms_process_state_change(&bq_charge_ok_signal_state,
				     gpio_pin_get_dt(&bq_chrg_ok))) {
		LOG_DBG("Detected a change in BQ_CHRG_OK, signal=%d",
			bq_charge_ok_signal_state.state);
		bq_charge_ok_signal_state.state_count++;
	}

	if (bms_process_state_change(&efuse_signal_state,
				     gpio_pin_get_dt(&efuse_pg_spec))) {
		LOG_DBG("Detected a change in EFUSE_PG, signal=%d",
			efuse_signal_state.state);
		efuse_signal_state.state_count++;
	}

	/* Dolos is considered ready when BB CHARGE OK signal is asserted. */
	if (bq_charge_ok_signal_state.state) {
		dled_ready_led_turn_on();
	} else {
		dled_ready_led_turn_off();
	}
}

bool bms_get_chrg_ok(void)
{
	return bq_charge_ok_signal_state.state;
}

bool bms_get_efuse_pg(void)
{
	return efuse_signal_state.state;
}

bool bms_get_sys_pres(void)
{
	if (sys_pres_is_floating) {
		return gpio_get_sys_pres() &&
		       smbus_target_is_comms_detected_short();
	}

	return gpio_get_sys_pres();
}

bool bms_get_sys_pres_raw(void)
{
	return gpio_pin_get_dt(&sys_pres_spec);
}

bool bms_get_sys_pres_pol(void)
{
	return sys_pres_pol;
}

bool bms_get_sys_pres_is_floating(void)
{
	return sys_pres_is_floating;
}

bool bms_is_sys_pres_forced(void)
{
	return force_sys_pres_flag;
}

enum bms_state bms_get_curr_state(void)
{
	const struct smf_state *current_state = state_data.ctx.current;

	for (size_t i = 0; i < ARRAY_SIZE(bms_states); i++) {
		if (current_state == &bms_states[i]) {
			return (enum bms_state)i;
		}
	}

	return BMS_STATE_INVALID;
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
			DOLOS_LOG_ERR(ERROR_BMS, ret,
				      "State machine failed to execute, err=%d",
				      ret);
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
		DOLOS_LOG_ERR(ERROR_BMS, ret,
			      "Failed to start BMS tick handler task, err=%d",
			      ret);
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

static int cmd_bms_sys_pres_force_handler(const struct shell *sh, size_t argc,
					  char **argv, void *data)
{
	if (strcmp("on", (char *)data) == 0) {
		force_sys_pres_flag = true;
		force_sys_pres_value = true;

		shell_print(sh, "Forcing system to be always present");
	} else if (strcmp("off", (char *)data) == 0) {
		force_sys_pres_flag = true;
		force_sys_pres_value = false;

		shell_print(sh, "Forcing system to be always absent");
	} else {
		force_sys_pres_flag = false;

		shell_print(sh, "Disabling the forced system present signal");
	}
	return 0;
}

static int cmd_bms_sys_pres_handler(const struct shell *sh, size_t argc,
				    char **argv)
{
	shell_print(sh, "System present polarity: %s%s",
		    sys_pres_pol ? "HIGH" : "LOW",
		    sys_pres_is_floating ? " (floating)" : "");

	shell_print(sh, "System present: %s %s",
		    bms_get_sys_pres() ? "Present" : "Not present",
		    force_sys_pres_flag ? "(forced)" : "");

	return 0;
}

int print_gpio_stats_handler(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "GPIO Statistics:");
	shell_print(sh, "   GPIO BQ_CHRG_OK int count     : %u",
		    bq_charge_ok_signal_state.state_count);
	shell_print(sh, "   GPIO SYSTEM_PRESENT int count : %u",
		    system_present_signal_state.state_count);
	shell_print(sh, "   GPIO EFUSE_PG                 : %u",
		    efuse_signal_state.state_count);
	return 0;
}

SHELL_SUBCMD_DICT_SET_CREATE(sys_pres_sub_cmd, cmd_bms_sys_pres_force_handler,
			     (on, "on", "Force system to be present"),
			     (off, "off", "Force system to be absent"),
			     (disable, "disable", "Disable the forced value"));

SHELL_CMD_REGISTER(sys_pres, &sys_pres_sub_cmd,
		   "Forces system present signal to set value (on/off/disable)",
		   cmd_bms_sys_pres_handler);
