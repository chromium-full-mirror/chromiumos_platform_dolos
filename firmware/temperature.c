/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <math.h>
#include <ti/driverlib/driverlib.h>
#include <stdbool.h>

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
#include "perf.h"

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
static bool pending_temp_calculation = false;
static bool pending_temp_conversion = false;

/* TEMP_ADC12 interrupt handler */
void TEMP_ADC12_INST_IRQHandler(void)
{
        PERF_RECORD_START(irq_temp_adc);

        switch (DL_ADC12_getPendingInterrupt(TEMP_ADC12_INST)) {
        case DL_ADC12_IIDX_MEM0_RESULT_LOADED: {
                /* Read the ADC value from the TEMP_SENSOR */
                dtemp_adc_code = (double)DL_ADC12_getMemResult(TEMP_ADC12_INST, DL_ADC12_MEM_IDX_0);

                pending_temp_calculation = true;
        }
        default: {
                break;
        }
        }

        PERF_RECORD_END(irq_temp_adc);
}

void temp_start_reading(void)
{
        if (!pending_temp_conversion) {
                DL_ADC12_enableConversions(TEMP_ADC12_INST);
                DL_ADC12_startConversion(TEMP_ADC12_INST);

                pending_temp_conversion = true;
        }
}

void temp_calculate(void)
{
        if (pending_temp_calculation) {
                double dtemp_voltage = (BIAS_VOLTAGE / ADC_RESOLUTION) * (double)dtemp_adc_code;
                dtemp_c = (THRM_A4 * pow(dtemp_voltage, 4)) + (THRM_A3 * pow(dtemp_voltage, 3)) +
                          (THRM_A2 * pow(dtemp_voltage, 2)) + (THRM_A1 * dtemp_voltage) + THRM_A0;
                dtemp_k = dtemp_c + KELVIN_ADDITION_FACTOR;

                pending_temp_calculation = false;
                pending_temp_conversion = false;
        }
}

double temp_get_k(void)
{
        return dtemp_k;
}

double temp_get_c(void)
{
        return dtemp_c;
}

void temp_print_readings(void)
{
        double dtemp_f = dtemp_c * (9.0 / 5.0) + 32;
        printf("TEMP_ADC12:\r\n");
        printf("   Raw ADC Temperature value    : %7d\r\n", dtemp_adc_code);
        printf("   Temperature in Celsius (C)   : %7.2f\r\n", dtemp_c);
        printf("   Temperature in Kelvin (K)    : %7.2f\r\n", dtemp_k);
        printf("   Temperature in Fahrenheit (F): %7.2f\r\n", dtemp_f);
}
