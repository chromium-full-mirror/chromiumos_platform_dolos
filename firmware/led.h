/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LED_H_
#define LED_H_

#include "ti_msp_dl_config.h"

enum led_dolos_status {
        LED_INPUT_POWER_STATUS = DOLOS_LEDS_GROUP_LED_ERROR_PIN,
        LED_DOLOS_PROGRAMMED = DOLOS_LEDS_GROUP_LED_BRICK_PIN,
        LED_DOLOS_READY = DOLOS_LEDS_GROUP_LED_READY_PIN,
        LED_DOLOS_POWERING = DOLOS_LEDS_GROUP_LED_PWRING_PIN
};

void led_turn_on(enum led_dolos_status status);
void led_turn_off(enum led_dolos_status status);
void led_toggle(enum led_dolos_status status);

#endif /* LED_H_ */
