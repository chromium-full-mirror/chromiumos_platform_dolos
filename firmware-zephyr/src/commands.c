/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>

/**
 * Handles the version command which prints the Dolos firmware version
 */
static int cmd_version_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Dolos version 1.0.0");

	return 0;
}

SHELL_CMD_REGISTER(version, NULL, "Prints Dolos version", cmd_version_handler);
