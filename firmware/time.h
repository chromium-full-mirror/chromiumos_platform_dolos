/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TIME_H_
#define TIME_H_

#include <stdint.h>

/** Delay the execution by given number of micro-seconds.
 */
void udelay(uint32_t usec);

/** Delay the execution by given number of milli-seconds.
 */
void mdelay(uint32_t msec);

/* Sleep for given number of seconds.
 */
void sleep(uint32_t seconds);

#endif /* TIME_H_ */
