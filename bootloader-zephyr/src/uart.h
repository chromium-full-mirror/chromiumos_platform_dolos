/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __UART_H__
#define __UART_H__

#include <zephyr/kernel.h>

/**
 * @brief Initializes the UART driver and ring buffers.
 */
void uart_init(void);

/**
 * @brief Sends a string over UART using the TX ring buffer.
 *
 * @param buf The string to send.
 */
void uart_print(const char *buf);

/**
 * @brief Receives a line from the RX ring buffer.
 *
 * This function waits for a complete line (terminated by '\r' or '\n')
 * to be available in the buffer.
 *
 * @param buf The buffer to store the received line.
 * @param size The size of the buffer.
 * @param timeout The timeout for waiting for data.
 * @return 0 on success, -EAGAIN on timeout.
 */
int uart_receive_line(char *buf, size_t size, k_timeout_t timeout);

#endif /* __UART_H__ */
