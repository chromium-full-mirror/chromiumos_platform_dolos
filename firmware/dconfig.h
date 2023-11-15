/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DCONFIG_H_
#define DCONFIG_H_

#include <stdbool.h>

/* Read config from flash and load it into memory variables. */
void dconfig_load(void);

/* changes the SYSTEM_PRESENT polarity and updates the flash. */
void dconfig_set_system_present_polarity(bool polarity);

/* Returns SYSTEM_PRESENT polarity, true for HIGH and false for LOW. */
bool dconfig_get_system_present_polarity(void);

#endif /* DCONFIG_H_ */
