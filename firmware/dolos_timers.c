/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dolos_timers.h"
#include "ti_msp_dl_config.h"
#include "dolos_gpio.h"
#include "printf.h"

void dtimers_init(void)
{
        DEBUG("Initializing Dolos Timer modules");

        /* Enable NVIC interrupts for EFUSE Timer */
        NVIC_ClearPendingIRQ(EFUSE_PG_TIMER_INST_INT_IRQN);
        NVIC_EnableIRQ(EFUSE_PG_TIMER_INST_INT_IRQN);
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
