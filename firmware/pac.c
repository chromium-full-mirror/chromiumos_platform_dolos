/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pac.h"
#include "dolos_smbus.h"
#include "printf.h"
#include "error.h"

#define PAC_SMBUS_ADDRESS 0x10
#define PAC_VOLTAGE_REGISTER 0x0F
#define PAC_CURRENT_REGISTER 0x13
#define PAC_REFRESH_REGISTER 0x00

#define PAC_REFRESH_DELAY 640000

static double voltage, current;

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
        voltage = ((double)data / ((1 << 16) - 1)) * 32;

        ret = dsb_controller_read_word(PAC_SMBUS_ADDRESS, PAC_CURRENT_REGISTER, (uint8_t *)&data);
        if (ret) {
                DEBUG("Failed to read current from PAC %d", ret);
                return DOLOS_ERROR_PAC;
        }
        current = ((double)data / ((1 << 16) - 1)) * 1000;

        DEBUG("Voltage :%f Current :%f", voltage, current);
        return DOLOS_SUCCESS;
}

uint32_t pac_read_voltage_mv(void)
{
        return voltage * 1000;
}

uint32_t pac_read_current_mv(void)
{
        return current * 1000;
}
