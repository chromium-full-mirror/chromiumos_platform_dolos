/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef BMS_H_
#define BMS_H_

#include <stdbool.h>

#include <zephyr/shell/shell.h>

/* BMS states */
enum bms_state {
	BMS_STATE_POWER_OUTPUT_OFF = 0,
	BMS_STATE_TIMER,
	BMS_STATE_POWER_GOOD_WAIT,
	BMS_STATE_POWER_OUTPUT_ON,
	BMS_STATE_COMM_POWER_OFF,
	BMS_STATE_INVALID,
};

#define EXPAND_ENUM_NAME(x) #x
#define ENUM_NAME(x) EXPAND_ENUM_NAME(x)

/**
 * @brief Returns BQ_CHRG_OK signal
 *
 * @return True if HIGH, false if LOW
 */
bool bms_get_chrg_ok(void);

/**
 * @brief Returns EFUSE_PG signal
 *
 * @return True if HIGH, false if LOW
 */
bool bms_get_efuse_pg(void);

/**
 * @brief Returns SYSTEM_PRESENT signal
 *
 * @return True if HIGH, false if LOW
 */
bool bms_get_sys_pres(void);

/**
 * @brief Is SYSTEM_PRESENT floating signal
 *
 * @return True if YES, false if NO
 */
bool bms_get_sys_pres_is_floating(void);

/**
 * @brief Returns raw SYSTEM_PRESENT signal, ignoring the forced value
 *
 * @return True if HIGH, false if LOW
 */
bool bms_get_sys_pres_raw(void);

/**
 * @brief Returns SYSTEM_PRESENT polarity
 *
 * @return True if HIGH, false if LOW
 */
bool bms_get_sys_pres_pol(void);

/**
 * @brief Checks if SYSTEM_PRESENT was forced
 *
 * @return True if forced, false otherwise
 */
bool bms_is_sys_pres_forced(void);

/**
 * @brief Returns the BMS state that is being executed
 *
 * @return current BMS state
 */
enum bms_state bms_get_curr_state(void);

int print_gpio_stats_handler(const struct shell *sh, size_t argc, char **argv);

bool bms_get_power_output_state(void);
#endif
