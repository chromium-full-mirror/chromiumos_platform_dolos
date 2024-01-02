/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DOLOS_UART_H_
#define DOLOS_UART_H_

/* Initializes Dolos UART modules */
void duart_init(void);

/* Returns a pointer to UART data if there's a pending command, otherwise returns NULL */
uint8_t *uart_get_pending_data(uint16_t *size);

/* Marks UART command as processed to allow receiving additional commands */
void uart_mark_data_processed();

/* Adds character to UART TX buffer, characters can be sent by calling  uart_drain_tx() */
void uart_async_send(uint8_t data);

/* Sends all the data in the UART TX buffer to the UART Interface */
void uart_drain_tx();

#endif
