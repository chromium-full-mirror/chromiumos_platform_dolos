/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

/**
 * @brief Processes a received command string.
 *
 * This function looks up the command in a command table and executes the
 * associated handler. If the command is not found, it prints an error.
 *
 * @param buf A null-terminated string containing the command to process.
 */
void process_command(char *buf);


#endif /* __COMMANDS_H__ */
