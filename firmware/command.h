/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_CMD_H_
#define DOLOS_CMD_H_

/* When called, handles pending UART command and allows receiving more commands */
void cmd_handle(void);

#endif
