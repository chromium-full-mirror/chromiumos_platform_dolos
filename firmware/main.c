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
#include "temperature.h"
#include "bms.h"

/* The corresponding function will be called every (100 * n)ms */
#define PAC_REFRESH_TIME 10
#define TEMP_READ_TIME 10
#define SB_UPDATE_REGISTERS_TIME 10
#define BMS_TICK_HANDLER_TIME 10

int main(void)
{
        SYSCFG_DL_init();

        /* A workaround for I2C CLK wedging issue */
        DL_I2C_disableTargetWakeup(DOLOS_SMBUS_TARGET_INST);

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
        cmd_init();

        sb_load_registers_from_dflash();

        uint32_t time_elapsed = 0;

        uint32_t pac_refresh_last_call_time = 0;
        uint32_t temp_read_last_call_time = 0;
        uint32_t sb_update_registers_last_call_time = 0;
        uint32_t bms_tick_handler_last_call_time = 0;

        while (1) {
                time_elapsed = dtimers_get_op_timer_elapsed();

                uart_drain_tx();
                cmd_process();

                if (time_elapsed - pac_refresh_last_call_time >= PAC_REFRESH_TIME) {
                        pac_refresh();
                        pac_refresh_last_call_time = time_elapsed;
                }

                if (time_elapsed - temp_read_last_call_time >= TEMP_READ_TIME) {
                        temp_start_reading();
                        temp_calculate();
                        temp_read_last_call_time = time_elapsed;
                }

                if (time_elapsed - sb_update_registers_last_call_time >= SB_UPDATE_REGISTERS_TIME) {
                        sb_update_registers();
                        sb_update_registers_last_call_time = time_elapsed;
                }

                if (time_elapsed - bms_tick_handler_last_call_time >= BMS_TICK_HANDLER_TIME) {
                        bms_tick_handler();
                        bms_tick_handler_last_call_time = time_elapsed;
                }
        }
}
