/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "utils.h"
#include "errno.h"
#include "stdlib.h"

enum parse_status parse_num(char *buff, int *x, int base)
{
        errno = 0;
        char *temp;
        *x = strtol(buff, &temp, base);
        if (temp == buff) {
                return PARSE_END_OF_STR;
        }
        if (errno == ERANGE) {
                return PARSE_OVERFLOW;
        }
        if (*temp != '\0') {
                return PARSE_LEFTOVER_JUNK;
        }
        return PARSE_SUCCESS;
}

enum parse_status parse_int(char *buff, int *x)
{
        return parse_num(buff, x, 10);
}

enum parse_status parse_hex(char *buff, int *x)
{
        return parse_num(buff, x, 16);
}
