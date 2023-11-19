/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>

#include "log.h"
#include "stats.h"

struct stats stats = { 0 };

void print_stats(void)
{
        print_smbus_stats();
        print_gpio_stats();
        print_uart_stats();
        print_cmd_stats();
        print_flash_stats();
}

void print_smbus_stats(void)
{
        printf("Host:\r\n");
        printf("   SMBus Int count               : %6d\r\n", stats.smbus_target_int_count);
        printf("   SMBus Read bytes              : %6d\r\n", stats.smbus_target_reg_read_bytes);
        printf("   SMBus Write bytes             : %6d\r\n", stats.smbus_target_reg_write_bytes);
        printf("   SMBus Int state:Ok            : %6d\r\n", stats.smbus_target_state_count[SMBus_State_OK]);
        printf("   SMBus Int state:DataSizeError : %6d\r\n", stats.smbus_target_state_count[SMBus_State_DataSizeError]);
        printf("   SMBus Int state:PECError      : %6d\r\n", stats.smbus_target_state_count[SMBus_State_PECError]);
        printf("   SMBus Int state:TimeoutError  : %6d\r\n", stats.smbus_target_state_count[SMBus_State_TimeOutError]);
        printf("   SMBus Int state:FirstByte     : %6d\r\n",
               stats.smbus_target_state_count[SMBus_State_Target_FirstByte]);
        printf("   SMBus Int state:ByteReceived  : %6d\r\n",
               stats.smbus_target_state_count[SMBus_State_Target_ByteReceived]);
        printf("   SMBus Int state:QCMD          : %6d\r\n", stats.smbus_target_state_count[SMBus_State_Target_QCMD]);
        printf("   SMBus Int state:CmdComplete   : %6d\r\n",
               stats.smbus_target_state_count[SMBus_State_Target_CmdComplete]);
        printf("   SMBus Int state:TargetError   : %6d\r\n", stats.smbus_target_state_count[SMBus_State_Target_Error]);
        printf("   SMBus Int state:NotReady      : %6d\r\n",
               stats.smbus_target_state_count[SMBus_State_Target_NotReady]);
        printf("   SMBus Int state:NTR           : %6d\r\n", stats.smbus_target_state_count[SMBus_State_Target_NTR]);
        printf("   SMBus Int state:ArbLost       : %6d\r\n",
               stats.smbus_target_state_count[SMBus_State_Controller_ArbLost]);
        printf("   SMBus Int state:ControllerErr : %6d\r\n",
               stats.smbus_target_state_count[SMBus_State_Controller_Error]);
        printf("   SMBus Int state:ControllerNACK: %6d\r\n",
               stats.smbus_target_state_count[SMBus_State_Controller_NACK]);
        printf("   SMBus Int state:Unknown       : %6d\r\n", stats.smbus_target_state_count[SMBus_State_Unknown]);
        printf("   SMBus Read success            : %6d\r\n", stats.smbus_target_reg_read_success);
        printf("   Smart Battery Read fail       : %6d\r\n", stats.smbus_target_reg_read_failures);
        printf("   SMBus Write success           : %6d\r\n", stats.smbus_target_reg_write_success);
        printf("   Smart Battery Write fail      : %6d\r\n", stats.smbus_target_reg_write_failures);
}

void print_gpio_stats(void)
{
        printf("GPIO:\r\n");
        printf("   GPIO BQ_CHRG_OK int count     : %6d\r\n", stats.dgpio_gpioa_bq_chrg_ok_int_count);
        printf("   GPIO SYSTEM_PRESENT int count : %6d\r\n", stats.dgpio_gpioa_system_present_int_count);
        printf("   GPIO EFUSE_PG                 : %6d\r\n", stats.dgpio_gpiob_efuse_pg_int_count);
}

void print_uart_stats(void)
{
        printf("UART0:\r\n");
        printf("   RX int count                  : %6d\r\n", stats.uart_rx_int_count);
        printf("   TX int count                  : %6d\r\n", stats.uart_tx_int_count);
        printf("   RX char count                 : %6d\r\n", stats.uart_rx_char_count);
        printf("   RX EOF count                  : %6d\r\n", stats.uart_rx_eof_count);
        printf("   RX scan count                 : %6d\r\n", stats.uart_rx_scan_count);
}

void print_cmd_stats(void)
{
        printf("CMD:\r\n");
        printf("   RX count                      : %6d\r\n", stats.cmd_rx_count);
        printf("   Invalid cmd count             : %6d\r\n", stats.cmd_inval_count);
}

void print_flash_stats(void)
{
        printf("FLASH:\r\n");
        printf("   Write successful              : %6d\r\n", stats.dflash_write_success);
        printf("   Write failure                 : %6d\r\n", stats.dflash_write_failure);
}
