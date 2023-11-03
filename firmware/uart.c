/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ti_msp_dl_config.h"
#include "log.h"

#include "stats.h"

#define UART_MAX_BUFFER 1000
static uint32_t uart_data_index;
static uint8_t uart_data[UART_MAX_BUFFER];
static bool uart_rx_processing_pending = false;

#define CMD_END 13

void duart_init(void)
{
        NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
        NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

void UART_0_INST_IRQHandler(void)
{
        int intr_type;
        uint32_t new_data_count;
        uint32_t remaining_buf_size;

        intr_type = DL_UART_Main_getPendingInterrupt(UART_0_INST);
        if (intr_type != DL_UART_MAIN_IIDX_RX) {
                return;
        }
        stats.uart_rx_int_count++;

        /* Wrap around if too much data is received (old data is discarded) */
        if (uart_data_index >= UART_MAX_BUFFER - 1) {
                uart_data_index = 0;
                return;
        }

        /* Discard the new data if waiting for processing to be done. */
        if (uart_rx_processing_pending) {
                return;
        }

        /* Receive data */
        remaining_buf_size = UART_MAX_BUFFER - uart_data_index;
        new_data_count = DL_UART_Main_drainRXFIFO(UART_0_INST, &uart_data[uart_data_index], remaining_buf_size);
        stats.uart_rx_char_count += new_data_count;
        /* If end of data is received mark processing is required. */
        for (uint16_t i = uart_data_index; i < uart_data_index + new_data_count; i++) {
                stats.uart_rx_scan_count++;
                if (uart_data[i] == CMD_END) {
                        uart_data[i] = '\0';
                        uart_rx_processing_pending = true;
                        stats.uart_rx_eof_count++;
                        break;
                } else {
                        uart_rx_processing_pending = false;
                }
        }

        uart_data_index += new_data_count;
}

uint8_t *uart_get_pending_data(uint16_t *size)
{
        if (uart_rx_processing_pending) {
                *size = uart_data_index;
                return uart_data;
        }
        return NULL;
}

void uart_mark_data_processed()
{
        uart_data_index = 0;
        uart_rx_processing_pending = false;
}
