/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_GPIO_H_
#define DOLOS_GPIO_H_

#include <stdbool.h>

/* Initializes Dolos GPIO ports */
void dgpio_init(void);

/* Enables Dolos output */
void dgpio_output_enable(void);

/* Disables Dolos output */
void dgpio_output_disable(void);

bool dgpio_get_system_present_signal(void);
bool dgpio_get_bq_charge_ok_signal(void);
bool dgpio_get_efuse_pg_signal(void);

void dgpio_efuse_pg_operation_reset(void);
void dgpio_efuse_pg_operation_disable(void);

#endif /* DOLOS_GPIO_H_ */
