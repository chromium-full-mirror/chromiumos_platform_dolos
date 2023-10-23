/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ti_msp_dl_config.h"
#include <ti/smbus/smbus.h>

#include "printf.h"
#include "error.h"

#include "dolos_smbus.h"

#define SMBUS_CONTROLLER_CLK 32000000
#define SMBUS_CONTROLLER_TIMEOUT 10000

static SMBus smbus_controller_inst;
static SMBus smbus_target_inst;

static volatile SMBus_State smbus_controller_bus_state;

static uint8_t smbus_target_rx_buffer[SMB_MAX_PACKET_SIZE];
static uint8_t smbus_target_tx_buffer[SMB_MAX_PACKET_SIZE];

void dsb_init(void)
{
        DEBUG("Initializing SMBus controller and SMBus target");
        /* Initiate SMBUS controller peripheral */
        SMBus_controllerInit(&smbus_controller_inst, DOLOS_SMBUS_CONTROLLER_INST, SMBUS_CONTROLLER_CLK);

        /* Initiate SMBUS target peripheral */
        SMBus_targetInit(&smbus_target_inst, DOLOS_SMBUS_TARGET_INST);
        SMBus_targetSetAddress(&smbus_target_inst, DOLOS_SMBUS_TARGET_TARGET_OWN_ADDR);
        SMBus_targetSetRxBuffer(&smbus_target_inst, smbus_target_rx_buffer, sizeof(smbus_target_rx_buffer));
        SMBus_targetSetTxBuffer(&smbus_target_inst, smbus_target_tx_buffer, sizeof(smbus_target_tx_buffer));
        SMBus_targetEnableInt(&smbus_target_inst);

        /* Enable NVIC interrupts for controller SMBus */
        NVIC_ClearPendingIRQ(DOLOS_SMBUS_CONTROLLER_INST_INT_IRQN);
        NVIC_EnableIRQ(DOLOS_SMBUS_CONTROLLER_INST_INT_IRQN);

        /* Enable NVIC interrupts for target SMBus */
        NVIC_ClearPendingIRQ(DOLOS_SMBUS_TARGET_INST_INT_IRQN);
        NVIC_EnableIRQ(DOLOS_SMBUS_TARGET_INST_INT_IRQN);
}

static inline int dsb_controller_read_internal(uint8_t target_address, bool is_block, uint8_t command_byte,
                                               uint8_t *rx_data, int rx_size)
{
        int8_t ret;
        int rx_len;

        /* Send command to the target */
        if (is_block) {
                DEBUG("Read block target(%#x), command: %#x", rx_size, target_address, command_byte);
                ret = SMBus_controllerReadBlock(&smbus_controller_inst, target_address, command_byte, rx_data);
        } else {
                DEBUG("Read %d target(%#x), command: %#x", rx_size, target_address, command_byte);
                ret = SMBus_controllerReadByteWord(&smbus_controller_inst, target_address, command_byte, rx_data,
                                                   rx_size);
        }
        if (ret != SMBUS_RET_OK) {
                ERROR("Failed to send command byte to target(%#x): %d", target_address, ret);
                return DOLOS_ERROR_SMBUS;
        }

        /* Wait until transfer is done, if it times out reset the controller */
        ret = SMBus_controllerWaitUntilDone(&smbus_controller_inst, SMBUS_CONTROLLER_TIMEOUT);
        if (ret != SMBUS_RET_OK) {
                ERROR("Timeout while waiting after command byte from target(%#x): %d", target_address, ret);
                SMBus_controllerReset(&smbus_controller_inst);
                return DOLOS_ERROR_TIMEOUT;
        }

        /* If transfer is over, read available data on the SMBus line and store it in controller RX Buff */
        if (smbus_controller_bus_state != SMBus_State_OK) {
                ERROR("Target(%#x) not in correct state: %d", target_address, smbus_controller_bus_state);
                return DOLOS_ERROR_SMBUS;
        }

        rx_len = SMBus_getRxPayloadAvailable(&smbus_controller_inst);
        DEBUG("Successfully read %d bytes from target(%#x)", target_address, rx_len);
        return DOLOS_SUCCESS;
}

int dsb_controller_read_block(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data)
{
        return dsb_controller_read_internal(target_address, true, command_byte, rx_data, 0);
}

int dsb_controller_read_byte(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data)
{
        return dsb_controller_read_internal(target_address, false, command_byte, rx_data, 1);
}

int dsb_controller_read_word(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data)
{
        return dsb_controller_read_internal(target_address, false, command_byte, rx_data, 2);
}

int dsb_controller_write(uint8_t target_address, uint8_t command_byte, uint8_t *tx_data, int tx_size)
{
        int ret;
        if (tx_data) {
                DEBUG("WriteByteWord:: sending command byte to target(%#x), command: %#x", tx_size, target_address,
                      command_byte);
                ret = SMBus_controllerWriteByteWord(&smbus_controller_inst, target_address, command_byte, tx_data,
                                                    tx_size);
        } else {
                DEBUG("SendByte:: sending command byte to target(%#x), command: %#x", tx_size, target_address,
                      command_byte);
                ret = SMBus_controllerSendByte(&smbus_controller_inst, target_address, command_byte);
        }
        if (ret != SMBUS_RET_OK) {
                ERROR("Failed to send command byte to target(%#x): %d", target_address, ret);
                return DOLOS_ERROR_SMBUS;
        }

        /* Wait until transfer is done, if it times out reset the controller */
        ret = SMBus_controllerWaitUntilDone(&smbus_controller_inst, SMBUS_CONTROLLER_TIMEOUT);
        if (ret != SMBUS_RET_OK) {
                ERROR("Timeout while waiting after command byte from target(%#x): %d", target_address, ret);
                SMBus_controllerReset(&smbus_controller_inst);
                return DOLOS_ERROR_TIMEOUT;
        }

        DEBUG("Wrote to target(%#x) to register %d", target_address, command_byte);
        return DOLOS_SUCCESS;
}

/* smbus_controller_inst interrupt handler */
void DOLOS_SMBUS_CONTROLLER_INST_IRQHandler(void)
{
        smbus_controller_bus_state = SMBus_controllerProcessInt(&smbus_controller_inst);
}

/* smbus_target_inst interrupt handler */
void DOLOS_SMBUS_TARGET_INST_IRQHandler(void)
{
        const SMBus_State bus_state = SMBus_targetProcessInt(&smbus_target_inst);
        SMBus_processDone(&smbus_target_inst);
}
