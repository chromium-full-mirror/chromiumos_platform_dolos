/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"
#include "smart_battery.h"

#include <stdlib.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/crc.h>

/**
 * EEPROM Data info
 */
#define EEPROM_SIZE DT_PROP(DT_NODELABEL(eeprom), size)

#define EEPROM_VERSION_OFFSET 0x0
#define EEPROM_SYS_PRES_POL_OFFSET 0x1
#define EEPROM_MANUFACTURER_YEAR_OFFSET 0x2
#define EEPROM_MANUFACTURER_WEEK_OFFSET 0x3
#define EEPROM_SERIAL_NO_OFFSET 0x4
#define EEPROM_CRC32_OFFSET 0x11
#define EEPROM_CRC32_VER_DATA_OFFSET 0x15

#define EEPROM_PAGE_SIZE 16
#define EEPROM_CRC32_VER_DATA_SIZE (EEPROM_SIZE - EEPROM_CRC32_VER_DATA_OFFSET)
#define EEPROM_WRITE_CYCLE_TIME_MS 5

LOG_MODULE_REGISTER(eeprom, LOG_LEVEL_INF);

static const struct i2c_dt_spec i2c_spec =
	I2C_DT_SPEC_GET(DT_NODELABEL(eeprom));

int eeprom_read(uint8_t addr, uint8_t *data)
{
	int ret;

	LOG_DBG("Starting 1 byte EEPROM read, addr=%#x", addr);

	ret = i2c_reg_read_byte_dt(&i2c_spec, addr, data);
	if (ret != 0) {
		LOG_WRN("Failed to read from EEPROM, addr=%#x, err=%d", addr,
			ret);
		return ret;
	}

	LOG_DBG("Read from EEPROM, addr=%#x, data=%#x", addr, *data);
	return 0;
}

int eeprom_read_n(uint8_t addr, uint8_t *data, size_t len)
{
	int ret;

	LOG_DBG("Starting %d byte EEPROM read, addr=%#x", len, addr);
	ret = i2c_write_read_dt(&i2c_spec, &addr, sizeof(addr), data, len);
	if (ret != 0) {
		LOG_WRN("Failed to read from EEPROM, addr=%#x, err=%d", addr,
			ret);
		return ret;
	}

	LOG_DBG("Read from EEPROM, addr=%#x", addr);
	LOG_HEXDUMP_DBG(data, len, "Data read:");

	return 0;
}

int eeprom_write(uint8_t addr, uint8_t data)
{
	int ret;

	LOG_DBG("Starting EEPROM write, addr=%#x, data=%#x", addr, data);
	ret = i2c_reg_write_byte_dt(&i2c_spec, addr, data);
	if (ret != 0) {
		LOG_WRN("Failed to write to EEPROM, addr=%#x, data=%#x err=%d",
			addr, data, ret);
		return ret;
	}

	LOG_DBG("Wrote to EEPROM, addr=%#x, data=%#x", addr, data);

	return 0;
}

int eeprom_write_n(uint8_t addr, uint8_t *data, size_t len)
{
	int ret;
	uint8_t write_buf[EEPROM_PAGE_SIZE + 1];
	uint8_t *data_ptr = data;
	size_t bytes_to_write = 0;

	LOG_DBG("Starting EEPROM write, addr=%#x", addr);
	LOG_HEXDUMP_DBG(data, len, "Writing data:");

	while (len > 0) {
		if (addr % EEPROM_PAGE_SIZE != 0) {
			bytes_to_write =
				MIN(len, EEPROM_PAGE_SIZE - (addr % 16));
		} else {
			bytes_to_write = MIN(len, EEPROM_PAGE_SIZE);
		}

		write_buf[0] = addr;
		memcpy(&write_buf[1], data_ptr, bytes_to_write);

		ret = i2c_write_dt(&i2c_spec, &write_buf[0],
				   bytes_to_write + 1);
		if (ret != 0) {
			LOG_WRN("Failed to write to EEPROM, addr=%#x, err=%d",
				addr, ret);
			LOG_HEXDUMP_WRN(write_buf, bytes_to_write + 1,
					"Page: ");
			return ret;
		}

		LOG_DBG("Wrote to EEPROM, addr=%#x", addr);
		LOG_HEXDUMP_DBG(write_buf, bytes_to_write + 1, "Page: ");

		data_ptr += bytes_to_write;
		addr += bytes_to_write;
		len -= bytes_to_write;

		k_sleep(K_MSEC(EEPROM_WRITE_CYCLE_TIME_MS));
	}

	return ret;
}

int eeprom_get_version(uint8_t *version)
{
	int ret;

	LOG_DBG("Reading EEPROM version");
	ret = eeprom_read(EEPROM_VERSION_OFFSET, version);
	if (ret != 0) {
		LOG_WRN("Failed to read EEPROM version, err=%d", ret);
		return ret;
	}

	LOG_DBG("Version: %d", *version);

	return 0;
}

int eeprom_get_sys_pres_pol(uint8_t *sys_pres_pol)
{
	int ret;

	LOG_DBG("Reading EEPROM system present polarity");
	ret = eeprom_read(EEPROM_SYS_PRES_POL_OFFSET, sys_pres_pol);
	if (ret != 0) {
		LOG_WRN("Failed to read EEPROM system present polarity, err=%d",
			ret);
		return ret;
	}

	LOG_DBG("System present polarity: %s",
		(*sys_pres_pol ? "HIGH" : "LOW"));

	return 0;
}

int eeprom_get_year(uint8_t *year)
{
	int ret;

	LOG_DBG("Reading EEPROM year");
	ret = eeprom_read(EEPROM_MANUFACTURER_YEAR_OFFSET, year);
	if (ret != 0) {
		LOG_WRN("Failed to read EEPROM year, err=%d", ret);
		return ret;
	}

	LOG_DBG("Year: %d", *year + 2000);

	return 0;
}

int eeprom_get_week(uint8_t *week)
{
	int ret;

	LOG_DBG("Reading EEPROM week");
	ret = eeprom_read(EEPROM_MANUFACTURER_WEEK_OFFSET, week);
	if (ret != 0) {
		LOG_WRN("Failed to read EEPROM week, err=%d", ret);
		return ret;
	}

	LOG_DBG("Week: %d", *week);

	return 0;
}

int eeprom_get_serial_no(uint16_t *serial_no)
{
	int ret;

	LOG_DBG("Reading EEPROM serial number");
	ret = eeprom_read_n(EEPROM_SERIAL_NO_OFFSET, (uint8_t *)serial_no,
			    sizeof(*serial_no));
	if (ret != 0) {
		LOG_WRN("Failed to read EEPROM serial number, err=%d", ret);
		return ret;
	}

	LOG_DBG("Serial number: 0x%04X", *serial_no);

	return 0;
}

int eeprom_get_crc32(uint32_t *crc32)
{
	int ret;

	LOG_DBG("Reading EEPROM CRC32");
	ret = eeprom_read_n(EEPROM_CRC32_OFFSET, (uint8_t *)crc32,
			    sizeof(*crc32));
	if (ret != 0) {
		LOG_WRN("Failed to read EEPROM CRC32, err=%d", ret);
		return ret;
	}

	LOG_DBG("CRC32: 0x%08X", *crc32);

	return 0;
}

int eeprom_crc32_check(void)
{
	uint8_t cr32_ver_data[EEPROM_CRC32_VER_DATA_SIZE];
	uint32_t crc32;
	uint32_t calculated_crc32;
	int ret;

	LOG_DBG("Starting EEPROM CRC32 check");
	ret = eeprom_get_crc32(&crc32);
	if (ret != 0) {
		LOG_ERR("Failed to perform CRC32 check, err=%d", ret);
		return ret;
	}

	ret = eeprom_read_n(EEPROM_CRC32_VER_DATA_OFFSET, cr32_ver_data,
			    EEPROM_CRC32_VER_DATA_SIZE);
	if (ret != 0) {
		LOG_ERR("Failed to read data to perform CRC32 check, err=%d",
			ret);
		return ret;
	}

	/* Bitwise NOT performed to obtain CRC32 JAMCRC */
	calculated_crc32 =
		~crc32_ieee(cr32_ver_data, EEPROM_CRC32_VER_DATA_SIZE);
	if (crc32 != calculated_crc32) {
		LOG_ERR("Incorrect CRC32, expected=0x%08X, got=0x%08X",
			calculated_crc32, crc32);
		return -1;
	}

	LOG_DBG("CRC32 check succeeded");
	return 0;
}

int eeprom_get_word_reg(uint8_t offset, uint16_t *reg_data)
{
	int ret;

	LOG_DBG("Reading word register at offset=%d", offset);
	ret = eeprom_read_n(offset, (uint8_t *)reg_data, 2);
	if (ret != 0) {
		LOG_WRN("Failed to read word register, err=%d", ret);
		return ret;
	}

	LOG_DBG("Read word register: 0x%04x", *reg_data);

	return 0;
}

int eeprom_get_block_reg(uint8_t offset, uint8_t *len, uint8_t *reg_data)
{
	int ret;

	LOG_DBG("Reading block register at offset=%d", offset);
	ret = eeprom_read(offset, len);

	if (ret != 0) {
		LOG_WRN("Failed to read block register length, err=%d", ret);
		return ret;
	}

	if (*len > SB_REG_DATA_MAX_SIZE || *len == 0) {
		LOG_WRN("Invalid register data length, len=%d, err=%d", *len,
			ret);
		return -EOVERFLOW;
	}

	ret = eeprom_read_n(offset + 1, reg_data, *len);
	if (ret != 0) {
		LOG_WRN("Failed to read block register data, err=%d", ret);
		return ret;
	}

	LOG_HEXDUMP_DBG(reg_data, *len, "Read block register:");

	return 0;
}

/**
 * Handles eeprom read shell command. Reads a single byte from EEPROM.
 */
static int cmd_eeprom_read_handler(const struct shell *sh, size_t argc,
				   char **argv)
{
	ARG_UNUSED(argc);

	uint8_t data;
	uint8_t addr;
	int err = 0;

	addr = shell_strtoul(argv[1], 16, &err);
	if (err != 0) {
		shell_error(sh, "Invaild address: %s", argv[1]);
		return 0;
	}

	if (eeprom_read(addr, &data) == 0) {
		shell_print(sh, "Read data=0x%02x from address=0x%02x", data,
			    addr);
	} else {
		shell_error(sh, "Failed to read data from EEPROM");
	}
	return 0;
}

/**
 * Handles eeprom readall shell command. Reads all EEPROM raw data.
 */
static int cmd_eeprom_readall_handler(const struct shell *sh, size_t argc,
				      char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint8_t data[256];

	if (eeprom_read_n(0, data, ARRAY_SIZE(data)) == 0) {
		shell_print(sh, "EEPROM data: ");
		shell_hexdump(sh, data, ARRAY_SIZE(data));

	} else {
		shell_error(sh, "Failed to read data from EEPROM");
	}
	return 0;
}

/**
 * Handles eeprom write shell command. Writes a single byte to EEPROM.
 */
static int cmd_eeprom_write_handler(const struct shell *sh, size_t argc,
				    char **argv)
{
	ARG_UNUSED(argc);

	uint8_t addr;
	uint8_t data;
	int err = 0;

	addr = shell_strtoul(argv[1], 16, &err);
	if (err != 0) {
		shell_error(sh, "Invalid address: %s", argv[1]);
		return 0;
	}

	data = shell_strtoul(argv[2], 16, &err);
	if (err != 0) {
		shell_error(sh, "Invalid data: %s", argv[2]);
		return 0;
	}

	if (eeprom_write(addr, data) == 0) {
		shell_print(sh, "Wrote data=0x%02x to address=0x%02x", data,
			    addr);
	} else {
		shell_error(sh, "Failed to write data to EEPROM");
	}
	return 0;
}

/**
 * Handles eeprom writen shell command. Writes n bytes to EEPROM.
 */
static int cmd_eeprom_writen_handler(const struct shell *sh, size_t argc,
				     char **argv)
{
	size_t len;
	uint8_t addr;
	uint8_t data[16];
	int err = 0;

	addr = shell_strtoul(argv[1], 16, &err);
	if (err != 0) {
		shell_error(sh, "Invalid address: %s", argv[1]);
		return 0;
	}

	len = shell_strtoul(argv[2], 10, &err);
	if (err != 0) {
		shell_error(sh, "Invalid length: %s", argv[2]);
		return 0;
	}

	if (argc != len + 3) {
		shell_error(sh,
			    "Invalid provided data length, expected %d entries",
			    len);
		return 0;
	}

	for (size_t i = 0; i < len; i++) {
		data[i] = shell_strtoul(argv[i + 3], 16, &err);

		if (err != 0) {
			shell_error(sh, "Invalid data byte: %d:%s", i,
				    argv[i + 3]);
			return 0;
		}
	}

	if (eeprom_write_n(addr, data, len) == 0) {
		shell_print(sh, "Wrote data to address=0x%02x", addr);
	} else {
		shell_error(sh, "Failed to write data to EEPROM");
	}
	return 0;
}

/**
 * Handles eeprom data shell command. Prints the EEPROM data block.
 */
static int cmd_eeprom_data_handler(const struct shell *sh, size_t argc,
				   char **argv)
{
	uint8_t version;
	uint8_t sys_pres_pol;
	uint8_t year;
	uint8_t week;
	uint16_t serial_no;
	uint32_t crc32;

	if (eeprom_get_version(&version) == 0) {
		shell_print(sh, "Version: %d", version);
	} else {
		shell_error(sh, "Failed to read version from EEPROM");
		return 0;
	}

	if (eeprom_get_sys_pres_pol(&sys_pres_pol) == 0) {
		shell_print(sh, "System present polarity: %s",
			    sys_pres_pol ? "HIGH" : "LOW");
	} else {
		shell_error(
			sh,
			"Failed to read system present polarity from EEPROM");
		return 0;
	}

	if (eeprom_get_year(&year) == 0) {
		shell_print(sh, "Year: %d", 2000 + year);
	} else {
		shell_error(sh, "Failed to read year from EEPROM");
		return 0;
	}

	if (eeprom_get_week(&week) == 0) {
		shell_print(sh, "Week: %d", week);
	} else {
		shell_error(sh, "Failed to read week from EEPROM");
		return 0;
	}

	if (eeprom_get_serial_no(&serial_no) == 0) {
		shell_print(sh, "Serial number: 0x%04X", serial_no);
	} else {
		shell_error(sh, "Failed to read serial number from EEPROM");
		return 0;
	}

	if (eeprom_get_crc32(&crc32) == 0) {
		shell_print(sh, "CRC32: 0x%08X", crc32);
	} else {
		shell_error(sh, "Failed to read CRC32 from EEPROM");
		return 0;
	}

	eeprom_crc32_check();

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	eeprom_sub_cmds,
	SHELL_CMD_ARG(read, NULL, "Reads a single byte",
		      cmd_eeprom_read_handler, 2, 0),
	SHELL_CMD_ARG(write, NULL, "Writes a single byte",
		      cmd_eeprom_write_handler, 3, 0),
	SHELL_CMD(data, NULL, "Reads EEPROM data block",
		  cmd_eeprom_data_handler),
	SHELL_CMD(readall, NULL, "Reads all EEPROM bytes",
		  cmd_eeprom_readall_handler),
	SHELL_CMD_ARG(writen, NULL, "Writes N bytes up to 16 bytes",
		      cmd_eeprom_writen_handler, 3, 16),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(eeprom, &eeprom_sub_cmds, "EEPROM command set", NULL);
