/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "led.h"
#include <ti/driverlib/driverlib.h>
#include "ti_msp_dl_config.h"

void led_turn_on(enum led_dolos_status status)
{
        DL_GPIO_clearPins(DOLOS_LEDS_GROUP_PORT, status);
}

void led_turn_off(enum led_dolos_status status)
{
        DL_GPIO_setPins(DOLOS_LEDS_GROUP_PORT, status);
}

void led_toggle(enum led_dolos_status status)
{
        DL_GPIO_togglePins(DOLOS_LEDS_GROUP_PORT, status);
}
