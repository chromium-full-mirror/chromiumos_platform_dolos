/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <string.h>

#include "dconfig.h"
#include "log.h"
#include "error.h"
#include "utils.h"
#include "dolos_flash.h"
#include "dolos_gpio.h"

#define SYSTEM_PRESENT_POLARITY_KEY 0x5000

/* True for HIGH and False for LOW */
static bool system_present_polarity = 1;

/* Saves SYSTEM_PRESENT polarity to flash */
static void dconfig_store_system_present_polarity_to_dflash(void)
{
        DEBUG("Saving SYSTEM_PRESENT polarity to flash");
        if (dflash_write_data(SYSTEM_PRESENT_POLARITY_KEY, (uint32_t)system_present_polarity) != DOLOS_SUCCESS) {
                DEBUG("Failed to save SYSTEM_PRESENT polarity to flash");
        }
}

/* Loads SYSTEM_PRESENT polarity from flash */
static void dconfig_load_system_present_polarity_from_dflash(void)
{
        DEBUG("Loading SYSTEM_PRESENT polarity from flash");
        uint32_t polarity = 0;

        if (!dflash_read_data(SYSTEM_PRESENT_POLARITY_KEY, &polarity)) {
                DEBUG("Failed to read SYSTEM_PRESENT polarity from flash, using default polarity %d", polarity);
        }
        dconfig_set_system_present_polarity(polarity);
}

void dconfig_set_system_present_polarity(bool polarity)
{
        DEBUG("Setting SYSTEM_PRESENT polarity");

        if (system_present_polarity == polarity) {
                return;
        }
        system_present_polarity = polarity;
        dconfig_store_system_present_polarity_to_dflash();
}

bool dconfig_get_system_present_polarity(void)
{
        return system_present_polarity;
}

void dconfig_load(void)
{
        DEBUG("Loading SYSTEM_PRESENT polarity from flash");
        dconfig_load_system_present_polarity_from_dflash();
}
