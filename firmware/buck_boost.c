/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "dolos_smbus.h"
#include "error.h"
#include "time.h"
#include "log.h"
#include "smart_battery.h"

#define BQ_I2C_ADDRESS 0x6b

enum bq_reg_space {
        BQ_REG_CHARGE_OPTION_0 = 0x00,
        BQ_REG_CHARGE_CURRENT = 0x02,
        BQ_REG_CHARGE_VOLTAGE = 0x04,
        BQ_REG_CHARGE_OPTION_1 = 0x30,
        BQ_REG_CHARGE_OPTION_2 = 0x32,
};

#define BQ_REG_CHARGE_OPTION_0_VALUE 0x010a
#define BQ_REG_CHARGE_CURRENT_VALUE 0x1800
#define BQ_REG_CHARGE_VOLTAGE_VALUE 0x20d0
#define BQ_REG_CHARGE_OPTION_1_VALUE 0x3700
#define BQ_REG_CHARGE_OPTION_2_VALUE 0x0037

#define BQ_REG_CHARGE_CURRENT_MASK
#define BQ_REG_CHARGE_VOLTAGE_MASK

struct bq_reg_config {
        enum bq_reg_space reg_address;
        uint16_t value;
};

/** Write to a bq register through SMBus
 */
static int bb_write_config(enum bq_reg_space reg_address, uint16_t value)
{
        int ret;
        uint16_t rx_buf;

        ret = dsb_controller_write(BQ_I2C_ADDRESS, reg_address, (uint8_t *)&value, 2);
        if (ret) {
                ERROR("Buck boost write failed - register(%#2x): %d", reg_address, ret);
                return ret;
        }

        ret = dsb_controller_read_word(BQ_I2C_ADDRESS, reg_address, (uint8_t *)&rx_buf);
        if (ret) {
                ERROR("Buck boost reading back failed - register(%#2x): %d", reg_address, ret);
                return ret;
        }
        return 0;
}

/* Update BQ register from Smart Battery register */
static int bb_update_register_from_sb(enum bq_reg_space bq_address, enum sb_register_address sb_address)
{
        uint16_t buff;
        uint8_t buff_len;
        int ret;

        ret = sb_read_register(sb_address, (uint8_t *)&buff, &buff_len);
        if (ret)
                return ret;

        bb_write_config(bq_address, buff);

        return 0;
}

/* Initialize the buckboost option registers to the following:
 * Update the BIT3 RSNS_RAC bit to use 10m ohm in the ChargeOption1 Register
 * Disable the  EN_EXTILIM bit in ChargeOption2 register to allow current output more than 1.5A since the default Input
 * ILMIT hardware setting is 1.5A
 */
int bb_init(void)
{
        int i, ret;
        uint8_t rx_buf[2];
        struct bq_reg_config config[] = { { BQ_REG_CHARGE_OPTION_0, BQ_REG_CHARGE_OPTION_0_VALUE },
                                          { BQ_REG_CHARGE_OPTION_1, BQ_REG_CHARGE_OPTION_1_VALUE },
                                          { BQ_REG_CHARGE_OPTION_2, BQ_REG_CHARGE_OPTION_2_VALUE } };

        DEBUG("Initializing buck boost");

        dsb_controller_read_word(BQ_I2C_ADDRESS, 0, (uint8_t *)&rx_buf);

        for (int i = 0; i < sizeof(config) / sizeof(config[0]); i++) {
                ret = bb_write_config(config[i].reg_address, config[i].value);
                if (ret)
                        return ret;
        }

        bb_update_register_from_sb(BQ_REG_CHARGE_VOLTAGE, SB_REG_DESIGN_VOLTAGE);
        bb_update_register_from_sb(BQ_REG_CHARGE_CURRENT, SB_REG_DESIGN_CAPACITY);

        DEBUG("Buck boost init successfully done");

        return 0;
}
