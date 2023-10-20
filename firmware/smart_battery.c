/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>

#include "pac.h"
#include "dolos_smbus.h"
#include "printf.h"
#include "error.h"
#include "smart_battery.h"

static struct sb_register sb_registers[] = {
        { SB_REG_MANUFACTURER_ACCESS, 2, { 0x83, 0x43 } },
        { SB_REG_REMAINING_CAPACITY_ALARM, 2, { 0x67, 0x02 } },
        { SB_REG_REMAINING_TIME_ALARM, 2, { 0x0a, 0x00 } },
        { SB_REG_BATTERY_MODE, 2, { 0x01, 0x60 } },
        { SB_REG_AT_RATE, 2, { 0x0, 0x0 } },
        { SB_REG_AT_RATE_TIME_TO_FULL, 2, { 0xff, 0xff } },
        { SB_REG_AT_RATE_TIME_TO_EMPTY, 2, { 0xff, 0xff } },
        { SB_REG_AT_RATE_OK, 2, { 0x01, 0x00 } },
        { SB_REG_TEMPERATURE, 2, { 0x24, 0x0c } },
        { SB_REG_VOLTAGE, 2, { 0xd4, 0x20 } },
        { SB_REG_CURRENT, 2, { 0x83, 0x0f } },
        { SB_REG_AVERAGE_CURRENT, 2, { 0xf1, 0x09 } },
        { SB_REG_MAX_ERROR, 2, { 0x01, 0x00 } },
        { SB_REG_RELATIVE_STATE_OF_CHARGE, 2, { 0x63, 0x00 } },
        { SB_REG_ABSOLUTE_STATE_OF_CHARGE, 2, { 0x58, 0x00 } },
        { SB_REG_REMAINING_CAPACITY, 2, { 0x16, 0x11 } },
        { SB_REG_FULL_CHARGE_CAPACITY, 2, { 0x2c, 0x15 } },
        { SB_REG_RUN_TIME_TO_EMPTY, 2, { 0xff, 0xff } },
        { SB_REG_AVERAGE_TIME_TO_EMPTY, 2, { 0xff, 0xff } },
        { SB_REG_AVERAGE_TIME_TO_FULL, 2, { 0xff, 0xff } },
        { SB_REG_CHARGING_CURRENT, 2, { 0x00, 0x00 } },
        { SB_REG_CHARGING_VOLTAGE, 2, { 0x00, 0x00 } },
        { SB_REG_BATTERY_STATUS, 2, { 0xe0, 0x00 } },
        { SB_REG_CYCLE_COUNT, 2, { 0x21, 0x00 } },
        { SB_REG_DESIGN_CAPACITY, 2, { 0x06, 0x18 } },
        { SB_REG_DESIGN_VOLTAGE, 2, { 0x14, 0x1e } },
        { SB_REG_SPECIFICATION_INFO, 2, { 0x31, 0x00 } },
        { SB_REG_MANUFACTURE_DATE, 2, { 0x5b, 0x52 } },
        { SB_REG_SERIAL_NUMBER, 2, { 0x9b, 0x00 } },
        { SB_REG_MANUFACTURER_NAME, 12, { 0x0b, 0x33, 0x33, 0x33, 0x2d, 0x41, 0x43, 0x2d, 0x44, 0x41, 0x2D, 0x41 } },
        { SB_REG_DEVICE_NAME, 0xa, { 0x09, 0x47, 0x48, 0x30, 0x32, 0x30, 0x34, 0x37, 0x58, 0x4c } },
        { SB_REG_DEVICE_CHEMISTRY, 5, { 0x04, 0x4c, 0x49, 0x4f, 0x4e } },
        { SB_REG_MANUFACTURER_DATA,
          0x11,
          { 0x10, 0x36, 0x4B, 0x51, 0x44, 0x43, 0x30, 0x41, 0x38, 0x31, 0x45, 0x56, 0x4B, 0x5A, 0x4A, 0x00, 0x01 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_5, 2, { 0x00 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_4, 2, { 0x00 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_3, 2, { 0x00 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_2, 2, { 0x10, 0x83 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_1, 2, { 0x10, 0x84 } },
        { SB_REG_SB_PACK_STATUS, 2, { 0x1 } },
        { SB_REG_SB_ALT_MANUFACTURER_ACCESS, 6, { 0x06, 0x54, 0x00, 0x87, 0x03, 0x00 } }

};

/** Return a smart battery register value.
 *
 */
int sb_read_register(enum sb_register_address address, uint8_t *buffer, uint8_t *buf_size)
{
        struct sb_register *reg;
        int i;

        if (address >= SB_REG_LAST) {
                return DOLOS_ERROR_OVERFLOW;
        }

        for (i = 0; i < sizeof(sb_registers) / sizeof(sb_registers[0]); i++) {
                reg = &sb_registers[i];
                if (reg->address != address) {
                        continue;
                }

                for (int j = 0; j < reg->length; j++) {
                        buffer[j] = reg->data[j];
                }
                *buf_size = reg->length;
                return DOLOS_SUCCESS;
        }

        return DOLOS_ERROR_UNIMPLEMENTED;
}
