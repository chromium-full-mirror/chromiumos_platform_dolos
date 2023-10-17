/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "printf.h"
#include "ti_msp_dl_config.h"

int main(void)
{
	SYSCFG_DL_init();

	DEBUG("Starting Dolos");

	while (1) {
	}
}
