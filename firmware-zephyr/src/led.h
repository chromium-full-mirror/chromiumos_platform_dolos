/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LED_H_
#define LED_H

#include <stdbool.h>

#include <zephyr/drivers/led.h>

void dled_error_led_turn_on();
void dled_error_led_turn_off();
bool dled_error_led_is_on();
void dled_program_led_turn_on();
void dled_program_led_turn_off();
bool dled_program_led_is_on();
void dled_ready_led_turn_on();
void dled_ready_led_turn_off();
bool dled_ready_led_is_on();
void dled_powering_led_turn_on();
void dled_powering_led_turn_off();
bool dled_powering_led_is_on();
#endif
