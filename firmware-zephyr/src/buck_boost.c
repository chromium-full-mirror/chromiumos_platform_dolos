/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "buck_boost.h"
#include "eeprom_layout.h"
#include "error.h"
#include "smart_battery.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#define DT_DRV_COMPAT ti_bq25731

static const struct i2c_dt_spec i2c_spec = I2C_DT_SPEC_INST_GET(0);
LOG_MODULE_REGISTER(buck_boost, LOG_LEVEL_DBG);

enum bq_reg_space {
	BQ_REG_CHARGE_OPTION_0 = 0x00,
	BQ_REG_CHARGE_CURRENT = 0x02,
	BQ_REG_CHARGE_VOLTAGE = 0x04,
	BQ_REG_CHARGE_OPTION_1 = 0x30,
	BQ_REG_CHARGE_OPTION_2 = 0x32,
};

#define BQ_REG_CHARGE_OPTION_0_VALUE 0x010a
#define BQ_REG_CHARGE_OPTION_1_VALUE 0x3700
#define BQ_REG_CHARGE_OPTION_2_VALUE 0x0037

#define BQ_REG_CHARGE_VOLTAGE_MASK 0x7ff8
#define BQ_REG_CHARGE_CURRENT_MASK 0x1fc0

#define BQ_REG_CHARGE_CURRENT_SHIFT 1

struct bq_reg_config {
	enum bq_reg_space reg_address;
	uint16_t value;
};

/** Write to a bq register through i2c
 */
static int bq25731_write_config(enum bq_reg_space reg_address, uint16_t value)
{
	int ret;

	uint8_t write_buf[3];

	write_buf[0] = reg_address;
	write_buf[1] = value;
	write_buf[2] = value >> 8;

	ret = i2c_write_dt(&i2c_spec, write_buf, sizeof(write_buf));
	if (ret) {
		DOLOS_LOG_ERR(ERROR_BUCKBOOST, ret,
			      "Buck boost write failed - register(%#2x): %d",
			      reg_address, ret);
		return ret;
	}

	return ret;
}

/**
 * @brief Writes BQ25731 ChargeCurrent register from DesignCapacity smart
 * battery register
 *
 * @retval 0 on succcess
 * @retval Negative on failure
 */
static int bq25731_write_charge_current_sb(void)
{
	uint16_t design_capacity;
	uint16_t charge_current;
	int ret;

	design_capacity = sb_get_design_capacity_mah();

	charge_current = (design_capacity >> BQ_REG_CHARGE_CURRENT_SHIFT) &
			 BQ_REG_CHARGE_CURRENT_MASK;
	ret = bq25731_write_config(BQ_REG_CHARGE_CURRENT, charge_current);

	if (ret != 0) {
		DOLOS_LOG_ERR(
			ERROR_BUCKBOOST, ret,
			"Failed write BQ_REG_CHARGE_CURRENT from smart battery register SB_DESIGN_CAPACITY, bq_addr=0x%02x, sb_addr=0x%02x, data=0x%04x, err=%d",
			BQ_REG_CHARGE_CURRENT, SB_DESIGN_CAPACITY,
			charge_current, ret);
		return ret;
	}

	LOG_DBG("Wrote BQ BQ_REG_CHARGE_CURRENT from smart battery register SB_DESIGN_CAPACITY, bq_addr=0x%02x, sb_addr=0x%02x, data=0x%04x",
		BQ_REG_CHARGE_CURRENT, SB_DESIGN_CAPACITY, charge_current);

	return 0;
}

/**
 * @brief Writes BQ25731 ChargeVoltage register from DesignVoltage smart
 * battery register
 *
 * @retval 0 on succcess
 * @retval Negative on failure
 */
static int bq25731_write_charge_voltage_sb(void)
{
	int ret;
	uint16_t design_voltage;
	uint16_t charge_voltage;

	ret = sb_reg_read(SB_DESIGN_VOLTAGE, (uint8_t *)&design_voltage, NULL);
	if (ret != 0) {
		DOLOS_LOG_ERR(
			ERROR_BUCKBOOST, ret,
			"Failed to read register SB_DESIGN_VOLTAGE from smart battery, err=%d",
			ret);
		return ret;
	}

	charge_voltage = design_voltage & BQ_REG_CHARGE_VOLTAGE_MASK;
	ret = bq25731_write_config(BQ_REG_CHARGE_VOLTAGE, charge_voltage);

	if (ret != 0) {
		DOLOS_LOG_ERR(
			ERROR_BUCKBOOST, ret,
			"Failed write BQ_REG_CHARGE_CURRENT from smart battery register SB_DESIGN_VOLTAGE, bq_addr=0x%02x, sb_addr=0x%02x, data=0x%04x, err=%d",
			BQ_REG_CHARGE_VOLTAGE, SB_DESIGN_VOLTAGE,
			charge_voltage, ret);
		return ret;
	}

	LOG_DBG("Wrote BQ BQ_REG_CHARGE_VOLTAGE from smart battery register SB_DESIGN_VOLTAGE, bq_addr=0x%02x, sb_addr=0x%02x, data=0x%04x",
		BQ_REG_CHARGE_VOLTAGE, SB_DESIGN_VOLTAGE, charge_voltage);

	return 0;
}

/* Initialize the buckboost option registers to the following:
 * Update the BIT3 RSNS_RAC bit to use 10m ohm in the ChargeOption1 Register
 * Disable the  EN_EXTILIM bit in ChargeOption2 register to allow current output
 * more than 1.5A since the default Input ILMIT hardware setting is 1.5A
 */
int bq25731_init(void)
{
	int ret;

	struct bq_reg_config config[] = {
		{ BQ_REG_CHARGE_OPTION_0, BQ_REG_CHARGE_OPTION_0_VALUE },
		{ BQ_REG_CHARGE_OPTION_1, BQ_REG_CHARGE_OPTION_1_VALUE },
		{ BQ_REG_CHARGE_OPTION_2, BQ_REG_CHARGE_OPTION_2_VALUE },
	};

	LOG_DBG("Initializing buck boost");

	for (int i = 0; i < ARRAY_SIZE(config); i++) {
		ret = bq25731_write_config(config[i].reg_address,
					   config[i].value);
		if (ret) {
			DOLOS_LOG_ERR(ERROR_BUCKBOOST, ret,
				      "Failed to initialize buck boost: %d.",
				      ret);
			return ret;
		}
	}

	/* Initialize BQ ChargeVoltage and ChargeCurrent registers from smart
	 * battery */
	bq25731_write_charge_voltage_sb();
	bq25731_write_charge_current_sb();

	LOG_DBG("Buck boost init successfully done");
	return ret;
}
