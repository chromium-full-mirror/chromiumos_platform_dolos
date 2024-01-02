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
#include "printf.h"
#include "stats.h"
#include "command.h"
#include "perf.h"

#define UART_MAX_BUFFER 128
#define UART_TX_BUFFER_SIZE 2048

static uint8_t uart_data[UART_MAX_BUFFER];

/* UART TX Buff */
static uint8_t uart_tx_buf[UART_TX_BUFFER_SIZE];
static volatile uint8_t *uart_tx_buf_producer = &uart_tx_buf[0];
static volatile uint8_t *uart_tx_buf_consumer = &uart_tx_buf[0];

void duart_init(void)
{
        NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
        NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

void uart_async_send(uint8_t data)
{
        *uart_tx_buf_producer = data;
        ++uart_tx_buf_producer;

        /* Cycle on overflow */
        if (uart_tx_buf_producer - uart_tx_buf >= UART_TX_BUFFER_SIZE) {
                uart_tx_buf_producer = &uart_tx_buf[0];
        }
}

void uart_drain_tx()
{
        while (uart_tx_buf_consumer != uart_tx_buf_producer) {
                DL_UART_transmitDataBlocking(UART_0_INST, *uart_tx_buf_consumer);
                ++uart_tx_buf_consumer;

                /* Cycle on overflow */
                if (uart_tx_buf_consumer - uart_tx_buf >= UART_TX_BUFFER_SIZE) {
                        uart_tx_buf_consumer = &uart_tx_buf[0];
                }
        }
}

void UART_0_INST_IRQHandler(void)
{
        PERF_RECORD_START(irq_uart);

        int intr_type;
        uint32_t new_data_count;
        uint32_t remaining_buf_size;

        intr_type = DL_UART_Main_getPendingInterrupt(UART_0_INST);
        if (intr_type != DL_UART_MAIN_IIDX_RX) {
                return;
        }
        stats.uart_rx_int_count++;

        /* Receive data */
        new_data_count = DL_UART_Main_drainRXFIFO(UART_0_INST, uart_data, UART_MAX_BUFFER);
        stats.uart_rx_char_count += new_data_count;
        /* If end of data is received mark processing is required. */
        for (uint16_t i = 0; i < new_data_count; i++) {
                stats.uart_rx_scan_count++;
                cmd_putc(uart_data[i]);
        }

        PERF_RECORD_END(irq_uart);
}
