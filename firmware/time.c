/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ti_msp_dl_config.h>
#include "time.h"

#define ONE_USECOND_DELAY 32

void udelay(uint32_t usec)
{
        delay_cycles(usec * ONE_USECOND_DELAY);
}

void mdelay(uint32_t msec)
{
        udelay(msec * 1000);
}

void sleep(uint32_t seconds)
{
        /* TODO - actually sleep instead of spinning */
        mdelay(1000);
}
