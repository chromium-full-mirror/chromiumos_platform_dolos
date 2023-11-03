/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LOG_H_
#define LOG_H_

#include <stdarg.h>
#include <stddef.h>

#include "printf.h"

enum log_levels { LOG_LEVEL_ERROR = 0, LOG_LEVEL_INFO, LOG_LEVEL_DEBUG };

extern enum log_levels log_level;

#define DEBUG(_fmt, ...)                                                                        \
        do {                                                                                    \
                if (log_level >= LOG_LEVEL_DEBUG)                                               \
                        printf("DBG:   %s:%d " _fmt "\r\n", __func__, __LINE__, ##__VA_ARGS__); \
        } while (0)

#define INFO(_fmt, ...)                                                                         \
        do {                                                                                    \
                if (log_level >= LOG_LEVEL_INFO)                                                \
                        printf("INFO:  %s:%d " _fmt "\r\n", __func__, __LINE__, ##__VA_ARGS__); \
        } while (0)

#define ERROR(_fmt, ...)                                                                        \
        do {                                                                                    \
                if (log_level >= LOG_LEVEL_ERROR)                                               \
                        printf("ERROR: %s:%d " _fmt "\r\n", __func__, __LINE__, ##__VA_ARGS__); \
        } while (0)

/** Set the logging level
 */
void log_set_level(enum log_levels level);

#endif
