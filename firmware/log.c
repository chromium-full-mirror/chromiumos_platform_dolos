/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "log.h"

enum log_levels log_level = LOG_LEVEL_ERROR;

void log_set_level(enum log_levels level)
{
        log_level = level;
}
