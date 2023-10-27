/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>

#include "printf.h"
#include "stats.h"

struct stats stats = { 0 };

void print_stats(void)
{
        print_smbus_stats();
        print_gpio_stats();
        print_uart_stats();
}

void print_smbus_stats(void)
{
        DEBUG("Host:");
        DEBUG("   SMBus Int count               : %6d", stats.smbus_target_int_count);
        DEBUG("   SMBus Read bytes              : %6d", stats.smbus_target_reg_read_bytes);
        DEBUG("   SMBus Int state:Ok            : %6d", stats.smbus_target_state_count[SMBus_State_OK]);
        DEBUG("   SMBus Int state:DataSizeError : %6d", stats.smbus_target_state_count[SMBus_State_DataSizeError]);
        DEBUG("   SMBus Int state:PECError      : %6d", stats.smbus_target_state_count[SMBus_State_PECError]);
        DEBUG("   SMBus Int state:TimeoutError  : %6d", stats.smbus_target_state_count[SMBus_State_TimeOutError]);
        DEBUG("   SMBus Int state:FirstByte     : %6d", stats.smbus_target_state_count[SMBus_State_Target_FirstByte]);
        DEBUG("   SMBus Int state:ByteReceived  : %6d",
              stats.smbus_target_state_count[SMBus_State_Target_ByteReceived]);
        DEBUG("   SMBus Int state:QCMD          : %6d", stats.smbus_target_state_count[SMBus_State_Target_QCMD]);
        DEBUG("   SMBus Int state:CmdComplete   : %6d", stats.smbus_target_state_count[SMBus_State_Target_CmdComplete]);
        DEBUG("   SMBus Int state:TargetError   : %6d", stats.smbus_target_state_count[SMBus_State_Target_Error]);
        DEBUG("   SMBus Int state:NotReady      : %6d", stats.smbus_target_state_count[SMBus_State_Target_NotReady]);
        DEBUG("   SMBus Int state:NTR           : %6d", stats.smbus_target_state_count[SMBus_State_Target_NTR]);
        DEBUG("   SMBus Int state:ArbLost       : %6d", stats.smbus_target_state_count[SMBus_State_Controller_ArbLost]);
        DEBUG("   SMBus Int state:ControllerErr : %6d", stats.smbus_target_state_count[SMBus_State_Controller_Error]);
        DEBUG("   SMBus Int state:ControllerNACK: %6d", stats.smbus_target_state_count[SMBus_State_Controller_NACK]);
        DEBUG("   SMBus Int state:Unknown       : %6d", stats.smbus_target_state_count[SMBus_State_Unknown]);
        DEBUG("   SMBus Read success            : %6d", stats.smbus_target_reg_read_success);
        DEBUG("   Smart Battery Read fail       : %6d", stats.smbus_target_reg_read_failures);
}

void print_gpio_stats(void)
{
        DEBUG("GPIO:");
        DEBUG("   GPIO BQ_CHRG_OK int count     : %6d", stats.dgpio_gpioa_bq_chrg_ok_int_count);
        DEBUG("   GPIO SYSTEM_PRESENT int count : %6d", stats.dgpio_gpioa_system_present_int_count);
        DEBUG("   GPIO EFUSE_PG                 : %6d", stats.dgpio_gpiob_efuse_pg_int_count);
}

void print_uart_stats(void)
{
        DEBUG("UART0:");
        DEBUG("   RX int count                  : %6d", stats.uart_rx_int_count);
        DEBUG("   TX int count                  : %6d", stats.uart_tx_int_count);
}
