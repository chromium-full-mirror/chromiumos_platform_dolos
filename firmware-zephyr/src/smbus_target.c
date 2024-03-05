/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "smart_battery.h"
#include "smbus_target.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(smbus_target, LOG_LEVEL_DBG);

static const struct device *smbus_target_dev =
	DEVICE_DT_GET(DT_NODELABEL(i2c1));

static int smbus_data_receive_cnt = 0;

static int smbus_stop_callback(struct i2c_target_config *cfg)
{
	return 0;
}

static int smbus_read_requested(struct i2c_target_config *cfg,
				uint8_t *init_val)
{
	int ret;

	if (sb_curr_reg_is_word()) {
		if (sb_curr_reg_read(init_val) == 0) {
			ret = 0;
		} else {
			ret = -1;
		}
	} else {
		if (sb_curr_reg_get_length(init_val) == 0) {
			ret = 0;
		} else {
			ret = -1;
		}
	}

	return ret;
}

static int smbus_read_processed(struct i2c_target_config *cfg,
				uint8_t *next_byte)
{
	int ret;

	if (sb_curr_reg_read(next_byte) == 0) {
		ret = 0;
	} else {
		ret = -1;
	}

	return ret;
}

static int smbus_write_requested(struct i2c_target_config *cfg)
{
	smbus_data_receive_cnt = 0;

	return 0;
}

static int smbus_write_received(struct i2c_target_config *cfg,
				uint8_t next_byte)
{
	int ret;
	++smbus_data_receive_cnt;

	if (smbus_data_receive_cnt == 1) {
		/* First byte is always the address, we set the sb_curr_reg_ptr
		 * to point to the new register */
		if (sb_curr_reg_ptr_set(next_byte) == 0) {
			ret = 0;
		} else {
			ret = -1;
		}
	} else if (smbus_data_receive_cnt == 2 && !sb_curr_reg_is_word()) {
		/* Second byte for block registers is always the block length,
		 * we will set the length of the block register */
		if (sb_curr_reg_set_length(next_byte) == 0) {
			ret = 0;
		} else {
			ret = -1;
		}
	} else {
		/* Any other byte should be a data byte */
		if (sb_curr_reg_write(next_byte) == 0) {
			ret = 0;
		} else {
			ret = -1;
		}
	}

	return ret;
}

static const struct i2c_target_callbacks smb_target_callbacks = {
	.write_requested = smbus_write_requested,
	.read_requested = smbus_read_requested,
	.write_received = smbus_write_received,
	.read_processed = smbus_read_processed,
	.stop = smbus_stop_callback,
};

static struct i2c_target_config smbus_target_config = {
	.flags = 0,
	.address = 0xb,
	.callbacks = &smb_target_callbacks,
};

int smbus_target_init(void)
{
	int ret;

	LOG_INF("Initializing SMBus target device");

	if (!device_is_ready(smbus_target_dev)) {
		LOG_ERR("SMBus device is not ready");
		return -1;
	}

	ret = i2c_target_register(smbus_target_dev, &smbus_target_config);
	if (ret != 0) {
		LOG_ERR("SMBus device could not be registered as target, err=%d",
			ret);
		return -1;
	}

	LOG_INF("Initialed SMBus target device successfully");
	return 0;
}
