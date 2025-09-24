/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "led.h"

static const struct device *leds = DEVICE_DT_GET(DT_NODELABEL(leds));

#define LED_ERR DT_NODE_CHILD_IDX(DT_NODELABEL(lederr))
#define LED_PRG DT_NODE_CHILD_IDX(DT_NODELABEL(ledprg))
#define LED_RDY DT_NODE_CHILD_IDX(DT_NODELABEL(ledrdy))
#define LED_PWR DT_NODE_CHILD_IDX(DT_NODELABEL(ledpwr))

/* Define LED state */
typedef struct {
	bool state;
} led_state_t;

enum led_index {
	ERROR_LED_INDEX,
	PROGRAMMED_LED_INDEX,
	READY_LED_INDEX,
	POWERING_LED_INDEX,
	NUM_LEDS
};

led_state_t led_states[NUM_LEDS] = {
	{ .state = false },
	{ .state = false },
	{ .state = false },
	{ .state = false },
};

void dled_error_led_turn_on()
{
	led_on(leds, LED_ERR);
	led_states[ERROR_LED_INDEX].state = true;
}

void dled_error_led_turn_off()
{
	led_off(leds, LED_ERR);
	led_states[ERROR_LED_INDEX].state = false;
}

bool dled_error_led_is_on()
{
	return led_states[ERROR_LED_INDEX].state;
}

void dled_program_led_turn_on()
{
	led_on(leds, LED_PRG);
	led_states[PROGRAMMED_LED_INDEX].state = true;
}

void dled_program_led_turn_off()
{
	led_off(leds, LED_PRG);
	led_states[PROGRAMMED_LED_INDEX].state = false;
}

bool dled_program_led_is_on()
{
	return led_states[PROGRAMMED_LED_INDEX].state;
}

void dled_ready_led_turn_on()
{
	led_on(leds, LED_RDY);
	led_states[READY_LED_INDEX].state = true;
}

void dled_ready_led_turn_off()
{
	led_off(leds, LED_RDY);
	led_states[READY_LED_INDEX].state = false;
}

bool dled_ready_led_is_on()
{
	return led_states[READY_LED_INDEX].state;
}

void dled_powering_led_turn_on()
{
	led_on(leds, LED_PWR);
	led_states[POWERING_LED_INDEX].state = true;
}

void dled_powering_led_turn_off()
{
	led_off(leds, LED_PWR);
	led_states[POWERING_LED_INDEX].state = false;
}

bool dled_powering_led_is_on()
{
	return led_states[POWERING_LED_INDEX].state;
}
