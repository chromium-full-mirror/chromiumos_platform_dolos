/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <ti/driverlib/dl_timerg.h>

#include "dolos_timers.h"
#include "ti_msp_dl_config.h"
#include "dolos_gpio.h"
#include "log.h"
#include "dolos_smbus.h"

/* Increments once every 100ms */
static uint32_t dolos_op_timer_elapsed = 0;

static uint8_t dolos_uptime_100ms = 0;
static uint8_t dolos_uptime_1s = 0;
static uint8_t dolos_uptime_1m = 0;
static uint8_t dolos_uptime_1h = 0;
static uint32_t dolos_uptime_1d = 0;

void dtimers_init(void)
{
        DEBUG("Initializing Dolos Timer modules");

        /* Enable NVIC interrupts for EFUSE Timer */
        NVIC_ClearPendingIRQ(EFUSE_PG_TIMER_INST_INT_IRQN);
        NVIC_EnableIRQ(EFUSE_PG_TIMER_INST_INT_IRQN);

        /* Enable NVIC interrupts for dolos operations timer */
        NVIC_ClearPendingIRQ(DOLOS_OP_TIMER_INST_INT_IRQN);
        NVIC_EnableIRQ(DOLOS_OP_TIMER_INST_INT_IRQN);

        /* Enable NVIC interrupts for dolos operations timer */
        NVIC_ClearPendingIRQ(DOLOS_SMBUS_COMMUNICATION_TIMER_INST_INT_IRQN);
        NVIC_EnableIRQ(DOLOS_SMBUS_COMMUNICATION_TIMER_INST_INT_IRQN);
}

uint32_t dtimers_get_op_timer_elapsed(void)
{
        return dolos_op_timer_elapsed;
}

void dtimers_get_uptime(uint8_t *seconds, uint8_t *minutes, uint8_t *hours, uint32_t *days)
{
        *seconds = dolos_uptime_1s;
        *minutes = dolos_uptime_1m;
        *hours = dolos_uptime_1h;
        *days = dolos_uptime_1d;
}

void dtimers_reset_smbus_communication_timer(void)
{
        DL_Timer_disableInterrupt(DOLOS_SMBUS_COMMUNICATION_TIMER_INST, DL_TIMER_IIDX_LOAD);
        DL_Timer_stopCounter(DOLOS_SMBUS_COMMUNICATION_TIMER_INST);
        DL_Timer_setTimerCount(DOLOS_SMBUS_COMMUNICATION_TIMER_INST, 0);

        DL_Timer_startCounter(DOLOS_SMBUS_COMMUNICATION_TIMER_INST);
        DL_Timer_enableInterrupt(DOLOS_SMBUS_COMMUNICATION_TIMER_INST, DL_TIMER_IIDX_LOAD);
}

/* EFUSE_PG_TIMER interrupt handler */
void EFUSE_PG_TIMER_INST_IRQHandler(void)
{
        switch (DL_TimerG_getPendingInterrupt(EFUSE_PG_TIMER_INST)) {
        case DL_TIMER_IIDX_ZERO: {
                /* Updates DOLOS_OUTPUT_ENABLE after 5 seconds of EFUSE_PG_PIN being low */
                const bool efuse_pg = DL_GPIO_readPins(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);

                if (efuse_pg) {
                        dgpio_output_enable();
                } else {
                        dgpio_output_disable();
                }

                /* Enable EFUSE_PG interrupts */
                DL_GPIO_clearInterruptStatus(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);
                DL_GPIO_enableInterrupt(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);
                break;
        }
        default: {
                break;
        }
        }
}

/* DOLOS_OP_TIMER interrupt handler */
void DOLOS_OP_TIMER_INST_IRQHandler(void)
{
        switch (DL_TimerG_getPendingInterrupt(DOLOS_OP_TIMER_INST)) {
        case DL_TIMER_IIDX_ZERO: {
                /* Update the Dolos operation timer */
                ++dolos_op_timer_elapsed;

                /* Update the Dolos up-time counters */
                ++dolos_uptime_100ms;

                if (dolos_uptime_100ms >= 10) {
                        ++dolos_uptime_1s;
                        dolos_uptime_100ms = 0;
                }

                if (dolos_uptime_1s >= 60) {
                        ++dolos_uptime_1m;
                        dolos_uptime_1s = 0;
                }

                if (dolos_uptime_1m >= 60) {
                        ++dolos_uptime_1h;
                        dolos_uptime_1m = 0;
                }

                if (dolos_uptime_1h >= 24) {
                        ++dolos_uptime_1d;
                        dolos_uptime_1h = 0;
                }

                break;
        }
        default: {
                break;
        }
        }
}

/* DOLOS_SMBUS_COMMUNICATION_TIMER interrupt handler */
void DOLOS_SMBUS_COMMUNICATION_TIMER_INST_IRQHandler(void)
{
        switch (DL_TimerG_getPendingInterrupt(DOLOS_SMBUS_COMMUNICATION_TIMER_INST)) {
        case DL_TIMER_IIDX_LOAD: {
                /* Update the Dolos operation timer */
                dsb_target_set_communication_state(false);

                break;
        }
        default: {
                break;
        }
        }
}
