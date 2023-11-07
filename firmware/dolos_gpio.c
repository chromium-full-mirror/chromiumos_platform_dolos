/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dolos_gpio.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <math.h>
#include "log.h"
#include "led.h"
#include "stats.h"
#include "error.h"
#include "smart_battery.h"

#define BIAS_VOLTAGE 3.3
#define ADC_RESOLUTION 4096
#define THRM_A0 -4.232811E+02
#define THRM_A1 4.728797E+02
#define THRM_A2 -1.988841E+02
#define THRM_A3 4.869521E+01
#define THRM_A4 -1.158754E+00
#define KELVIN_ADDITION_FACTOR 273.15

static uint16_t dtemp_adc_code;
static double dtemp_c;
static double dtemp_k;

/* Returns true if system is present */
bool dgpio_get_system_present_signal(void)
{
        return !DL_GPIO_readPins(DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PORT, DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN);
}

/* Returns true if buck-boost charge ok is good */
bool dgpio_get_bq_charge_ok_signal(void)
{
        return DL_GPIO_readPins(BQ25731_GROUP_CHRG_OK_PORT, BQ25731_GROUP_CHRG_OK_PIN);
}

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

        /* Update DOLOS_OUTPUT_ENABLE if initially SYSTEM_PRESENT and BQ_CHRG_OK were good */
        dgpio_output_update_with_efuse_pg_operation();

        /* Enable LED_DOLOS_READY if BQ_CHRG_OK was good initially */
        const bool bq_chrg_ok = dgpio_get_bq_charge_ok_signal();

        if (bq_chrg_ok) {
                led_turn_on(LED_DOLOS_READY);
        } else {
                led_turn_off(LED_DOLOS_READY);
        }
}

/* Resets EFUSE_PG signal for operation */
void dgpio_efuse_pg_operation_reset(void)
{
        DL_GPIO_disableInterrupt(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);

        DL_TimerG_clearInterruptStatus(EFUSE_PG_TIMER_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        DL_TimerG_enableInterrupt(EFUSE_PG_TIMER_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        DL_TimerG_startCounter(EFUSE_PG_TIMER_INST);
}

/* Disables EFUSE_PG signal operation */
void dgpio_efuse_pg_operation_disable(void)
{
        DL_GPIO_disableInterrupt(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);

        DL_TimerG_disableInterrupt(EFUSE_PG_TIMER_INST, DL_TIMERG_INTERRUPT_ZERO_EVENT);
        DL_TimerG_stopCounter(EFUSE_PG_TIMER_INST);
}

/* Enables Dolos output */
void dgpio_output_enable(void)
{
        DL_GPIO_setPins(DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PORT, DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PIN);

        led_turn_on(LED_DOLOS_POWERING);
}

/* Disables Dolos output */
void dgpio_output_disable(void)
{
        DL_GPIO_clearPins(DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PORT, DOLOS_CONTROL_PINS_DOLOS_OUTPUT_ENABLE_PIN);

        led_turn_off(LED_DOLOS_POWERING);
}

bool dgpio_output_update(void)
{
        const bool system_present = dgpio_get_system_present_signal();
        const bool bq_chrg_ok = dgpio_get_bq_charge_ok_signal();

        if (system_present && bq_chrg_ok) {
                dgpio_output_enable();
                return true;
        } else {
                dgpio_output_disable();
                return false;
        }
}

void dgpio_output_update_with_efuse_pg_operation(void)
{
        if (dgpio_output_update()) {
                dgpio_efuse_pg_operation_reset();
        } else {
                dgpio_efuse_pg_operation_disable();
        }
}

void dgpio_read_temp(void)
{
        DL_ADC12_enableConversions(TEMP_ADC12_INST);
        DL_ADC12_startConversion(TEMP_ADC12_INST);
}

double dgpio_get_temp_k(void)
{
        return dtemp_k;
}

double dgpio_get_temp_c(void)
{
        return dtemp_c;
}

void dgpio_print_temp_readings(void)
{
        double dtemp_f = dtemp_c * (9.0 / 5.0) + 32;
        printf("TEMP_ADC12:\r\n");
        printf("   Raw ADC Temperature value    : %7d\r\n", dtemp_adc_code);
        printf("   Temperature in Celsius (C)   : %7.2f\r\n", dtemp_c);
        printf("   Temperature in Kelvin (K)    : %7.2f\r\n", dtemp_k);
        printf("   Temperature in Fahrenheit (F): %7.2f\r\n", dtemp_f);
}

/* GPIOA interrupt handler
 * Includes interrupts for BQ25731_GROUP_CHRG_OK_PIN and SYSTEM_PRESENT_PIN
 */
void group1_gpioa_interrupt_handler(void)
{
        /* Get all pending interrupts on port A */
        const uint32_t gpioa_interrupt_status = DL_GPIO_getEnabledInterruptStatus(
                GPIOA, DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN | BQ25731_GROUP_CHRG_OK_PIN);
        bool should_update_dolos_output = false;

        /* BQ25731_GROUP_CHRG_OK_PIN interrupt handler
         * Updates the DOLOS_OUTPUT_ENABLE signal
         */
        if ((gpioa_interrupt_status & BQ25731_GROUP_CHRG_OK_PIN) == BQ25731_GROUP_CHRG_OK_PIN) {
                should_update_dolos_output = true;
                const bool bq_chrg_ok = dgpio_get_bq_charge_ok_signal();

                if (bq_chrg_ok) {
                        led_turn_on(LED_DOLOS_READY);
                } else {
                        led_turn_off(LED_DOLOS_READY);
                }

                stats.dgpio_gpioa_bq_chrg_ok_int_count++;
                DL_GPIO_clearInterruptStatus(GPIOA, BQ25731_GROUP_CHRG_OK_PIN);
        }

        /* SYSTEM_PRESENT_PIN interrupt handler
         * Updates the DOLOS_OUTPUT_ENABLE signal
         */
        if ((gpioa_interrupt_status & DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN) == DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN) {
                should_update_dolos_output = true;

                stats.dgpio_gpioa_system_present_int_count++;
                DL_GPIO_clearInterruptStatus(GPIOA, DOLOS_CONTROL_PINS_SYSTEM_PRESENT_PIN);
        }

        /* Update Dolos output if SYSTEM_PRESENT_PIN and BQ25731_GROUP_CHRG_OK_PIN change state */
        if (should_update_dolos_output) {
                dgpio_output_update_with_efuse_pg_operation();
        }
}

void group1_gpiob_interrupt_handler(void)
{
        /* Get all pending interrupts on port B */
        const uint32_t gpiob_interrupt_status = DL_GPIO_getEnabledInterruptStatus(GPIOB, EFUSE_GROUP_EFUSE_PG_PIN);

        /* EFUSE_PG_PIN interrupt handler
         * Updates the DOLOS_OUTPUT_ENABLE signal
         */
        if ((gpiob_interrupt_status & EFUSE_GROUP_EFUSE_PG_PIN) == EFUSE_GROUP_EFUSE_PG_PIN) {
                const bool efuse_pg = DL_GPIO_readPins(EFUSE_GROUP_PORT, EFUSE_GROUP_EFUSE_PG_PIN);

                if (efuse_pg) {
                        dgpio_output_enable();
                } else {
                        dgpio_output_disable();
                }

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

/* TEMP_ADC12 interrupt handler */
void TEMP_ADC12_INST_IRQHandler(void)
{
        switch (DL_ADC12_getPendingInterrupt(TEMP_ADC12_INST)) {
        case DL_ADC12_IIDX_MEM0_RESULT_LOADED: {
                /* Read the ADC value from the TEMP_SENSOR */
                dtemp_adc_code = (double)DL_ADC12_getMemResult(TEMP_ADC12_INST, DL_ADC12_MEM_IDX_0);

                /* Calculate temperature using 4th order polynomial regression */
                double dtemp_voltage = (BIAS_VOLTAGE / ADC_RESOLUTION) * (double)dtemp_adc_code;
                dtemp_c = (THRM_A4 * pow(dtemp_voltage, 4)) + (THRM_A3 * pow(dtemp_voltage, 3)) +
                          (THRM_A2 * pow(dtemp_voltage, 2)) + (THRM_A1 * dtemp_voltage) + THRM_A0;
                dtemp_k = dtemp_c + KELVIN_ADDITION_FACTOR;
                break;
        }
        default: {
                break;
        }
        }
}
