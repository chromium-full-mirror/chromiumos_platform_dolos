/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef BMS_H_
#define BMS_H_

#include <stdint.h>

#include "dolos_smbus.h"

/* Handle BMS state after each tick
 */
void bms_tick_handler(void);

/* Handle efuse generated interrupt
 */
void bms_handle_efuse_interrupt(void);

/* Handle SYSTEM PRESET interrupt
 */
void bms_handle_system_present_interrupt(void);

/* Handle BUCK BOOOST interrupt
 */
void bms_handle_bb_charge_ok_interrupt(void);

/* Enables Dolos output and turn on the LED */
void bms_enable_output(void);

/* Disables Dolos output and turns of the LED */
void bms_disable_output(void);

/* Print state information about all signals*/
void bms_print_state_trackers(void);

void bms_force_output(bool on);

#endif /* BUCK_BOOST_H_ */
