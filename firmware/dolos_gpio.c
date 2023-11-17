/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <math.h>
#include <ti/driverlib/driverlib.h>

#include "dolos_gpio.h"
#include "ti_msp_dl_config.h"
#include "log.h"
#include "led.h"
#include "stats.h"
#include "error.h"
#include "smart_battery.h"
#include "utils.h"
#include "dolos_flash.h"
#include "string.h"
#include "dconfig.h"
#include "bms.h"

void dgpio_init(void)
{
        DEBUG("Initializing Dolos GPIO modules");

        /* We only care about EFUSE_PG_PIN interrupts after 5 seconds of enabling DOLOS_OUTPUT_ENABLE */
        DL_GPIO_disableInterrupt(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);

        /* Enable NVIC interrupts for GPIOA */
        NVIC_ClearPendingIRQ(GPIOA_INT_IRQn);
        NVIC_EnableIRQ(GPIOA_INT_IRQn);

        /* Enable NVIC interrupts for GPIOB */
        NVIC_ClearPendingIRQ(GPIOB_INT_IRQn);
        NVIC_EnableIRQ(GPIOB_INT_IRQn);

        /* Enable NVIC interrupts for TEMP_ADC12 */
        NVIC_ClearPendingIRQ(TEMP_ADC12_INST_INT_IRQN);
        NVIC_EnableIRQ(TEMP_ADC12_INST_INT_IRQN);

        DL_GPIO_initDigitalInputFeatures(DOLOS_CONTROL_PINS_SYSTEM_PRESENT_IOMUX, DL_GPIO_INVERSION_DISABLE,
                                         DL_GPIO_RESISTOR_PULL_UP, DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

bool dgpio_get_system_present_signal(void)
{
        if (dconfig_get_system_present_polarity()) {
                return DL_GPIO_readPins(DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PORT, DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN);
        } else {
                return !DL_GPIO_readPins(DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PORT, DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN);
        }
}

bool dgpio_get_bq_charge_ok_signal(void)
{
        return DL_GPIO_readPins(BQ25731_GROUP_CHRG_OK_PORT, BQ25731_GROUP_CHRG_OK_PIN);
}

bool dgpio_get_efuse_pg_signal(void)
{
        return DL_GPIO_readPins(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);
}

void dgpio_output_enable(void)
{
        DL_GPIO_setPins(DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PORT, DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PIN);
}

void dgpio_output_disable(void)
{
        DL_GPIO_clearPins(DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PORT, DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PIN);
}

void dgpio_efuse_pg_operation_reset(void)
{
        DL_GPIO_disableInterrupt(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);

        DL_TimerG_clearInterruptStatus(EFUSE_PG_TIMER_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        DL_TimerG_enableInterrupt(EFUSE_PG_TIMER_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        DL_TimerG_startCounter(EFUSE_PG_TIMER_INST);
}

void dgpio_efuse_pg_operation_disable(void)
{
        DL_GPIO_disableInterrupt(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);

        DL_TimerG_disableInterrupt(EFUSE_PG_TIMER_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        DL_TimerG_stopCounter(EFUSE_PG_TIMER_INST);
}

/* GPIOA interrupt handler
 * Includes interrupts for BQ25731_GROUP_CHRG_OK_PIN and SYSTEM_PRESENT_PIN
 */
void group1_gpioa_interrupt_handler(void)
{
        /* Get all pending interrupts on port A */
        const uint32_t gpioa_interrupt_status = DL_GPIO_getEnabledInterruptStatus(
                GPIOA, DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN | BQ25731_GROUP_CHRG_OK_PIN);

        /* BQ25731_GROUP_CHRG_OK_PIN interrupt handler
         */
        if ((gpioa_interrupt_status & BQ25731_GROUP_CHRG_OK_PIN) == BQ25731_GROUP_CHRG_OK_PIN) {
                stats.dgpio_gpioa_bq_chrg_ok_int_count++;
                DL_GPIO_clearInterruptStatus(GPIOA, BQ25731_GROUP_CHRG_OK_PIN);
                bms_handle_system_present_interrupt();
        }

        /* SYSTEM_PRESENT_PIN interrupt handler
         */
        if ((gpioa_interrupt_status & DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN) == DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN) {
                stats.dgpio_gpioa_system_present_int_count++;
                DL_GPIO_clearInterruptStatus(GPIOA, DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN);
                bms_handle_bb_charge_ok_interrupt();
        }
}

void group1_gpiob_interrupt_handler(void)
{
        /* Get all pending interrupts on port B */
        const uint32_t gpiob_interrupt_status = DL_GPIO_getEnabledInterruptStatus(GPIOB, EFUSE_GROUP_EFUSE_PG_PIN);

        /* EFUSE_PG_PIN interrupt handler
         */
        if ((gpiob_interrupt_status & EFUSE_GROUP_EFUSE_PG_PIN) == EFUSE_GROUP_EFUSE_PG_PIN) {
                bms_handle_efuse_interrupt();
                stats.dgpio_gpiob_efuse_pg_int_count++;
                DL_GPIO_clearInterruptStatus(GPIOB, EFUSE_GROUP_EFUSE_PG_PIN);
        }
}

/* GROUP1 interrupt handler (handles GPIOA and GPIOB interrupts)*/
void GROUP1_IRQHandler(void)
{
        switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        case DL_INTERRUPT_GROUP1_IIDX_GPIOA: {
                group1_gpioa_interrupt_handler();
                break;
        }
        case DL_INTERRUPT_GROUP1_IIDX_GPIOB: {
                group1_gpiob_interrupt_handler();
                break;
        }
        default: {
                break;
        }
        }
}
