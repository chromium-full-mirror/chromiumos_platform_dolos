/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#include <stdbool.h>

/* Reads the temperature of the Dolos from the thermistor */
void temp_start_reading(void);

/* Reads Dolos temperature in Kelvin */
double temp_get_k(void);

/* Reads Dolos temperature in Celsius */
double temp_get_c(void);

/* Prints temperature data to UART interface */
void temp_print_readings(void);

/* Calculate temperature using 4th order polynomial regression */
void temp_calculate(void);

#endif /* TEMPERATURE_H_ */
