/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

/**
 * @brief Gets the most recent temperature value in Kelvin
 *
 * @return Temperature in Kelvin
 */
double temperature_get_k(void);

/**
 * @brief Gets the most recent temperature value in Celsius
 *
 * @return Temperature in Celsius
 */
double temperature_get_c(void);

/**
 * @brief Gets the most recent temperature value in Fahrenheit
 *
 * @return Temperature in Fahrenheit
 */
double temperature_get_f(void);

#endif /* TEMPERATURE_H_ */
