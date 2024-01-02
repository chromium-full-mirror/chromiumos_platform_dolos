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

/* Increments once every 100ms */
static uint32_t dolos_op_timer_elapsed = 0;

void dtimers_init(void)
{
        DEBUG("Initializing Dolos Timer modules");

        /* Enable NVIC interrupts for EFUSE Timer */
        NVIC_ClearPendingIRQ(EFUSE_PG_TIMER_INST_INT_IRQN);
        NVIC_EnableIRQ(EFUSE_PG_TIMER_INST_INT_IRQN);

        /* Enable NVIC interrupts for dolos operations timer */
        NVIC_ClearPendingIRQ(DOLOS_OP_TIMER_INST_INT_IRQN);
        NVIC_EnableIRQ(DOLOS_OP_TIMER_INST_INT_IRQN);
}

uint32_t dtimers_get_op_timer_elapsed(void)
{
        return dolos_op_timer_elapsed;
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
                ++dolos_op_timer_elapsed;
                break;
        }
        default: {
                break;
        }
        }
}
