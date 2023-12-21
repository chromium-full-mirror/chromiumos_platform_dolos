/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef UTILS_H_
#define UTILS_H_

enum parse_status { PARSE_SUCCESS, PARSE_END_OF_STR, PARSE_OVERFLOW, PARSE_LEFTOVER_JUNK };

/* Parses string to long with defined base */
enum parse_status parse_num(char *buff, int *x, int base);

/* Parses string to long (treats string as integer) */
enum parse_status parse_int(char *buff, int *x);

/* Parses string to long (treats string as hex) */
enum parse_status parse_hex(char *buff, int *x);

#endif /* UTILS_H_ */
