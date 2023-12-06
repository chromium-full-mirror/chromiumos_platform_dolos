/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <string.h>

#include "eeprom.h"
#include "error.h"
#include "dolos_smbus.h"
#include "utils.h"
#include "time.h"

#define EEPROM_ADDRESS 0x50
#define EEPROM_SERIAL_ADDRESS 0x8

int eeprom_read_all(uint8_t *data)
{
        int ret;

        for (size_t i = 0; i < EEPROM_MAX_SIZE; i++) {
                ret = dsb_controller_read_byte(EEPROM_ADDRESS, (uint8_t)i, &data[i]);
                if (ret != DOLOS_SUCCESS) {
                        return ret;
                }
        }

        return DOLOS_SUCCESS;
}

int eeprom_read(uint8_t address, uint8_t *data)
{
        int ret;

        ret = dsb_controller_read_byte(EEPROM_ADDRESS, address, data);
        if (ret != DOLOS_SUCCESS) {
                return ret;
        }

        return DOLOS_SUCCESS;
}

int eeprom_write(uint8_t address, uint8_t data)
{
        int ret;

        ret = dsb_controller_write(EEPROM_ADDRESS, address, &data, 1);
        if (ret != DOLOS_SUCCESS) {
                return ret;
        }

        return DOLOS_SUCCESS;
}

int eeprom_serial_read(char *serial)
{
        int ret;

        for (size_t i = 0; i < EEPROM_SERIAL_SIZE; i++) {
                ret = eeprom_read(EEPROM_SERIAL_ADDRESS + i, (uint8_t *)&serial[i]);
                if (ret != DOLOS_SUCCESS) {
                        return ret;
                }
        }

        return DOLOS_SUCCESS;
}

int eeprom_serial_write(char *serial)
{
        int ret;

        for (size_t i = 0; i < EEPROM_SERIAL_SIZE; i++) {
                ret = eeprom_write(EEPROM_SERIAL_ADDRESS + i, (uint8_t)serial[i]);
                if (ret != DOLOS_SUCCESS) {
                        return ret;
                }

                /* Wait 2 ms until EEPROM writes the previous byte */
                mdelay(2);
        }

        return DOLOS_SUCCESS;
}

bool eeprom_serial_validate(char *serial)
{
        /* Verify serial length */
        if (strlen(serial) != EEPROM_SERIAL_SIZE) {
                return false;
        }

        return true;
}
