/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ti_msp_dl_config.h"
#include "ti/smbus/smbus.h"
#include "log.h"
#include "error.h"
#include "stats.h"
#include "dolos_smbus.h"
#include "smart_battery.h"
#include "perf.h"
#include "dolos_timers.h"

#define SMBUS_CONTROLLER_CLK 32000000
#define SMBUS_CONTROLLER_TIMEOUT 10000

static SMBus smbus_controller_inst;
static SMBus smbus_target_inst;

static volatile SMBus_State smbus_controller_bus_state;

static uint8_t smbus_target_rx_buffer[SMB_MAX_PACKET_SIZE];
static uint8_t smbus_target_tx_buffer[SMB_MAX_PACKET_SIZE];

/* Communication state for DOLOS_SMBUS_TARGET, should be cleared if there's no communication for 10 seconds by
 * DOLOS_SMBUS_COMMUNICATION_TIMER and set immediately when we receive an interrupt on the DOLOS_SMBUS_TARGET interface
 */
static bool target_communication_state = false;

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
                                               uint8_t *rx_data, int rx_size, size_t *rx_len)
{
        int8_t ret;

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
                SMBus_controllerReset(&smbus_controller_inst);
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
                SMBus_controllerReset(&smbus_controller_inst);
                return DOLOS_ERROR_SMBUS;
        }

        *rx_len = SMBus_getRxPayloadAvailable(&smbus_controller_inst);
        DEBUG("Successfully read %d bytes from target(%#x)", *rx_len, target_address);
        return DOLOS_SUCCESS;
}

int dsb_controller_read_block(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data, size_t *rx_len)
{
        return dsb_controller_read_internal(target_address, true, command_byte, rx_data, 0, rx_len);
}

int dsb_controller_read_byte(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data)
{
        return dsb_controller_read_internal(target_address, false, command_byte, rx_data, 1, NULL);
}

int dsb_controller_read_word(uint8_t target_address, uint8_t command_byte, uint8_t *rx_data)
{
        return dsb_controller_read_internal(target_address, false, command_byte, rx_data, 2, NULL);
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
                SMBus_controllerReset(&smbus_controller_inst);
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

/** Handles SMBus read requests for smart battery registers.
 *
 *  Find the value in the register array and write it in the smbus.
 */
static void dsb_handle_sb_smbus_read(SMBus *smbus, uint8_t cmd)
{
        int written_bytes = 0;

        written_bytes = sb_register_smbus_read_handler(cmd);
        if (written_bytes == 0) {
                stats.smbus_target_reg_read_failures++;
                return;
        }

        stats.smbus_target_reg_read_success++;
        stats.smbus_target_reg_read_bytes += written_bytes;
}

static void dsb_handle_sb_smbus_write(SMBus *smbus, uint8_t cmd)
{
        int read_bytes = 0;

        read_bytes = sb_register_smbus_write_handler(cmd);
        if (read_bytes == 0) {
                stats.smbus_target_reg_write_failures++;
                return;
        }
        stats.smbus_target_reg_write_success++;
        stats.smbus_target_reg_write_bytes += read_bytes;
}

void dsb_target_write_word(const uint8_t *data)
{
        smbus_target_inst.nwk.txLen = 2;
        smbus_target_inst.nwk.txBuffPtr[0] = data[0];
        smbus_target_inst.nwk.txBuffPtr[1] = data[1];
}

void dsb_target_write_block(const uint8_t *data, const uint8_t length)
{
        smbus_target_inst.nwk.txLen = length + 1;
        smbus_target_inst.nwk.txBuffPtr[0] = length;
        for (size_t i = 0; i < length; i++) {
                smbus_target_inst.nwk.txBuffPtr[i + 1] = data[i];
        }
}

void dsb_target_read_word(uint8_t *data)
{
        data[0] = smbus_target_inst.nwk.rxBuffPtr[1];
        data[1] = smbus_target_inst.nwk.rxBuffPtr[2];
}

void dsb_target_read_block(uint8_t *data, uint8_t *length)
{
        *length = smbus_target_inst.nwk.rxBuffPtr[1];
        for (size_t i = 0; i < *length; i++) {
                data[i] = smbus_target_inst.nwk.rxBuffPtr[i + 2];
        }
}

void dsb_target_reset(void)
{
        DL_I2C_reset(DOLOS_SMBUS_TARGET_INST);
        DL_I2C_disablePower(DOLOS_SMBUS_TARGET_INST);
        DL_I2C_enablePower(DOLOS_SMBUS_TARGET_INST);
        delay_cycles(POWER_STARTUP_DELAY);
        SYSCFG_DL_DOLOS_SMBUS_TARGET_init();

        /* A workaround for I2C CLK wedging issue */
        DL_I2C_disableTargetWakeup(DOLOS_SMBUS_TARGET_INST);
}

void dsb_target_set_communication_state(bool state)
{
        target_communication_state = state;
}

bool dsb_target_get_communication_state(void)
{
        return target_communication_state;
}

static uint8_t last_rx_cmd;

/* smbus_target_inst interrupt handler */
void DOLOS_SMBUS_TARGET_INST_IRQHandler(void)
{
        PERF_RECORD_START(irq_target_smbus);

        /* Mark that communication was received on the target interface and reset the DOLOS_SMBUS_COMMUNICATON_TIMER */
        target_communication_state = true;
        dtimers_reset_smbus_communication_timer();

        /* Handle target interrupts */
        SMBus_State bus_state;

        stats.smbus_target_int_count++;

        bus_state = SMBus_targetProcessInt(&smbus_target_inst);
        if (bus_state < SMBus_State_Unknown) {
                stats.smbus_target_state_count[bus_state]++;
        } else {
                stats.smbus_target_state_count[SMBus_State_Unknown]++;
        }
        switch (bus_state) {
        case SMBus_State_Target_FirstByte:
                last_rx_cmd = SMBus_targetGetCommand(&smbus_target_inst);
                break;
        case SMBus_State_Target_CmdComplete:
                if (smbus_target_inst.nwk.eState == SMBus_NwkState_TX_Resp) {
                        dsb_handle_sb_smbus_read(&smbus_target_inst, last_rx_cmd);
                } else {
                        if (sb_is_register_read_only(last_rx_cmd)) {
                                break;
                        }
                        dsb_handle_sb_smbus_write(&smbus_target_inst, last_rx_cmd);
                }
                break;
        default:
                break;
        }

        SMBus_processDone(&smbus_target_inst);

        PERF_RECORD_END(irq_target_smbus);
}
