/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ti_msp_dl_config.h"

/* NMI interrupt handler */
void NMI_Handler(void)
{
        while (1) {
                /* Do Nothing */
        }
}

/* Hard fault interrupt handler */
extern void HardFault_Handler(void)
{
        while (1) {
                /* Do Nothing */
        }
}

/* SVC interrupt handler */
extern void SVC_Handler(void)
{
        while (1) {
                /* Do Nothing */
        }
}

/* Pended Supervisor interrupt handler */
extern void PendSV_Handler(void)
{
        while (1) {
                /* Do Nothing */
        }
}

/* SysTick interrupt handler */
extern void SysTick_Handler(void)
{
        while (1) {
                /* Do Nothing */
        }
}
