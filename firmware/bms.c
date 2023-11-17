/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "error.h"
#include "log.h"
#include "led.h"
#include "dolos_gpio.h"
#include "dconfig.h"

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
static struct bms_signal_state_tracker bb_charge_ok_signal_state = {
        .state = false,
        .ticks = { 0 },
        .min_ticks = { 1, 1 },
};

static bool force_turn_on = false;

static void inline bms_print_state_tracker(char *name, bool polarity, struct bms_signal_state_tracker *tracker,
                                           bool cur_state)
{
        printf("%20s (%11s): SW:%d HW: %d Ticks:: LOW:%2d  HIGH:%2d\n\r", name, polarity ? "ACTIVE_HIGH" : "ACTIVE_LOW",
               tracker->state, cur_state, tracker->ticks[0], tracker->ticks[1]);
}

void bms_print_state_trackers(void)
{
        bms_print_state_tracker("SYSTEM PRESENT", dconfig_get_system_present_polarity(), &system_present_signal_state,
                                dgpio_get_system_present_signal());
        bms_print_state_tracker("BB CHARGE OK", true, &bb_charge_ok_signal_state, dgpio_get_bq_charge_ok_signal());
        bms_print_state_tracker("EFUSE PG", true, &efuse_signal_state, dgpio_get_efuse_pg_signal());
}

/** Process signal state change.
 */
static bool bms_process_state_change(struct bms_signal_state_tracker *tracker, bool new_state)
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

void bms_handle_efuse_interrupt(void)
{
        bms_process_state_change(&efuse_signal_state, dgpio_get_efuse_pg_signal());
        if (!efuse_signal_state.state) {
                dgpio_output_disable();
                led_turn_off(LED_DOLOS_POWERING);
        }
}

void bms_force_output(bool on)
{
        force_turn_on = on;
}

void bms_handle_system_present_interrupt(void)
{
}

void bms_handle_bb_charge_ok_interrupt(void)
{
}

static bool prev_output_state = false;
void bms_tick_handler(void)
{
        bool dolos_ready, enable_output;

        /* Check and change SW states if HW signal is stable for at least given number of seconds. */
        bms_process_state_change(&system_present_signal_state, dgpio_get_system_present_signal());
        bms_process_state_change(&bb_charge_ok_signal_state, dgpio_get_bq_charge_ok_signal());
        bms_process_state_change(&efuse_signal_state, dgpio_get_efuse_pg_signal());

        /* Dolos is considered ready when BB CHARGE OK signal is asserted. */
        dolos_ready = bb_charge_ok_signal_state.state;
        if (dolos_ready) {
                led_turn_on(LED_DOLOS_READY);
        } else {
                led_turn_off(LED_DOLOS_READY);
        }

        /* Enable output only if SYSTEM PRESENT is detected and dolos is ready. */
        enable_output = system_present_signal_state.state && dolos_ready;
        if (!enable_output && force_turn_on) {
                if (!prev_output_state) {
                        printf("Force turning on output\n\r");
                }
                enable_output = true;
        }
        prev_output_state = enable_output;

        if (enable_output) {
                dgpio_output_enable();
                dgpio_efuse_pg_operation_reset();
                led_turn_on(LED_DOLOS_POWERING);
        } else {
                dgpio_output_disable();
                dgpio_efuse_pg_operation_disable();
                led_turn_off(LED_DOLOS_POWERING);
        }
}
