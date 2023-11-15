/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_CMD_H_
#define DOLOS_CMD_H_

/** Initializes command modules
 */
void cmd_init(void);

/** Adds a character received in the input stream(UART) to command stream.
 *
 */
void cmd_putc(char data);

/** Process the characters in the command stream.
 */
void cmd_process(void);

#endif
