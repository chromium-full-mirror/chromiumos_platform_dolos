/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include "dolos_flash.h"
#include "ti/eeprom/emulation_type_b/eeprom_emulation_type_b.h"
#include "log.h"
#include "stats.h"
#include "error.h"

void dflash_init(void)
{
        DEBUG("Initializing Dolos Flash module");
        /* Initialize EEPROM Emulation */
        uint32_t EEPROMEmulationState = EEPROM_TypeB_init();
        if (EEPROMEmulationState == EEPROM_EMULATION_INIT_ERROR ||
            EEPROMEmulationState == EEPROM_EMULATION_TRANSFER_ERROR) {
                ERROR("Failed to Initialize EERPOM Emulation: %d", EEPROMEmulationState);
                __BKPT(0);
        }
}

/* Erases the group on the flash if erase flag is set and automatically starts a group transfer operation */
static void erase_group_if_necessary(void)
{
        if (gEEPROMTypeBEraseFlag == 1) {
                EEPROM_TypeB_eraseGroup();
                gEEPROMTypeBEraseFlag = 0;
        }
}

int dflash_write_data(uint16_t key, uint32_t data)
{
        DEBUG("Writing data to flash: key = %#x, data = %#x", key, data);
        uint32_t EEPROMEmulationState;

        EEPROMEmulationState = EEPROM_TypeB_write(key, data);
        if (EEPROMEmulationState != EEPROM_EMULATION_WRITE_OK) {
                stats.dflash_write_failure++;
                ERROR("Failed to write data to flash: %d", EEPROMEmulationState);
                return DOLOS_ERROR_FLASH;
        }

        /* Erase group after write if necessary*/
        erase_group_if_necessary();

        DEBUG("Write successful");
        stats.dflash_write_success++;
        return DOLOS_SUCCESS;
}

bool dflash_read_data(uint16_t key, uint32_t *data)
{
        DEBUG("Reading data from flash: key = %#x", key);
        *data = EEPROM_TypeB_readDataItem(key);
        if (gEEPROMTypeBSearchFlag == 0) {
                /* Register data not found */
                stats.dflash_read_failure++;
                return false;
        }

        DEBUG("Reading successful: data = %#x", *data);
        stats.dflash_read_success++;
        return true;
}
