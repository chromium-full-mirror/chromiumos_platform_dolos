/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ti_msp_dl_config.h"

#include "log.h"
#include "led.h"
#include "dolos_smbus.h"
#include "pac.h"
#include "dolos_gpio.h"
#include "dolos_timers.h"
#include "stats.h"
#include "time.h"
#include "buck_boost.h"
#include "smart_battery.h"
#include "command.h"
#include "uart.h"
#include "dolos_flash.h"
#include "dconfig.h"

int main(void)
{
        SYSCFG_DL_init();

        DEBUG("Starting Dolos");

        led_turn_off(LED_INPUT_POWER_STATUS);
        led_turn_off(LED_DOLOS_READY);
        led_turn_off(LED_DOLOS_POWERING);
        led_turn_on(LED_DOLOS_PROGRAMMED);

        dflash_init();
        dconfig_load();
        dsb_init();
        dtimers_init();
        bb_init();
        dgpio_init();
        duart_init();

        while (1) {
                pac_refresh();
                dgpio_read_temp();
                sb_update_registers();
                cmd_handle();
                sleep(1);
        }
}
