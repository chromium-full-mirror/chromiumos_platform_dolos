/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pac.h"
#include "dolos_smbus.h"
#include "log.h"
#include "error.h"

#define PAC_SMBUS_ADDRESS 0x10
#define PAC_VOLTAGE_REGISTER 0x0F
#define PAC_CURRENT_REGISTER 0x13
#define PAC_REFRESH_REGISTER 0x00

#define PAC_REFRESH_DELAY 640000

static double voltage_mv, current_ma;

static inline uint16_t swap(uint16_t data)
{
        return (data >> 8) | ((data & 0xff) << 8);
}

int pac_init(void)
{
        DEBUG("Initializing PAC");
        return DOLOS_SUCCESS;
}

int pac_refresh(void)
{
        int ret;
        uint16_t data;

        DEBUG("Refreshing PAC");
        ret = dsb_controller_write(PAC_SMBUS_ADDRESS, PAC_REFRESH_REGISTER, NULL, 0);
        if (ret) {
                DEBUG("Failed to refresh PAC registers %d", ret);
                return DOLOS_ERROR_PAC;
        }
        delay_cycles(PAC_REFRESH_DELAY);

        ret = dsb_controller_read_word(PAC_SMBUS_ADDRESS, PAC_VOLTAGE_REGISTER, (uint8_t *)&data);
        if (ret) {
                DEBUG("Failed to read voltage from PAC %d", ret);
                return DOLOS_ERROR_PAC;
        }
        voltage_mv = (double)swap(data) * 0.488288701f;

        ret = dsb_controller_read_word(PAC_SMBUS_ADDRESS, PAC_CURRENT_REGISTER, (uint8_t *)&data);
        if (ret) {
                DEBUG("Failed to read current from PAC %d", ret);
                return DOLOS_ERROR_PAC;
        }
        current_ma = (double)swap(data) * 1.5259021896696423f;

        DEBUG("Voltage :%5.0fmv Current :%5.0fma", voltage_mv, current_ma);
        return DOLOS_SUCCESS;
}

uint32_t pac_read_voltage_mv(void)
{
        return voltage_mv;
}

uint32_t pac_read_current_ma(void)
{
        return current_ma;
}

void pac_print_readings(void)
{
        printf("PAC:\r\n");
        printf("   Voltage:%5.0fmV\r\n", voltage_mv);
        printf("   Current:%5.0fmA\r\n", current_ma);
}
