/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_GPIO_H_
#define DOLOS_GPIO_H_

#include <stdbool.h>

/* Initializes Dolos GPIO ports */
void dgpio_init(void);

/* Enables Dolos output if buck-boost charging is good and system is present and returns true if output was enabled */
bool dgpio_output_update(void);

/* Enables Dolos output */
void dgpio_output_enable(void);

/* Disables Dolos output */
void dgpio_output_disable(void);

/* Attempts to enable Dolos output and resets the EFUSE_PG signal operation if output was enabled */
void dgpio_output_update_with_efuse_pg_operation(void);

/* Reads the temperature of the Dolos from the thermistor */
void dgpio_read_temp(void);

/* Reads Dolos temperature in Kelvin */
double dgpio_get_temp_k(void);

/* Reads Dolos temperature in Celsius */
double dgpio_get_temp_c(void);

#endif /* DOLOS_GPIO_H_ */
