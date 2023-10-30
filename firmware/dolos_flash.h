/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_FLASH_H_
#define DOLOS_FLASH_H_

#include <smart_battery.h>

/* Initializes Dolos Flash module */
void dflash_init(void);

/* Writes data to the flash key, returns DOLOS_SUCCESS if write was successful and DOLOS_ERROR_FLASH on failure */
int dflash_write_data(uint16_t key, uint32_t data);

/* Reads data from the flash key, returns true if read was successful */
bool dflash_read_data(uint16_t key, uint32_t *data);

#endif /* DOLOS_FLASH_H_ */
