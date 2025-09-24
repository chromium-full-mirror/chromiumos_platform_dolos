/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "eeprom.h"
#include "eeprom_layout.h"
#include "error.h"
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

#define EEPROM_VERSION_OFFSET offsetof(struct eeprom_data, version)
#define EEPROM_SYS_PRES_POL_OFFSET offsetof(struct eeprom_data, polarity)
#define EEPROM_MANUFACTURER_YEAR_OFFSET \
	offsetof(struct eeprom_data, manufactured_year)
#define EEPROM_MANUFACTURER_WEEK_OFFSET \
	offsetof(struct eeprom_data, manufactured_week)
#define EEPROM_SERIAL_NO_OFFSET offsetof(struct eeprom_data, serial_number)
#define EEPROM_CRC32_OFFSET offsetof(struct eeprom_data, crc)
#define EEPROM_CRC32_VER_DATA_OFFSET \
	(offsetof(struct eeprom_data, crc) + sizeof(uint32_t))

#define EEPROM_PAGE_SIZE 16
#define EEPROM_CHUNK_SIZE 256

#define EEPROM_CRC32_VER_DATA_SIZE (EEPROM_SIZE - EEPROM_CRC32_VER_DATA_OFFSET)
#define EEPROM_WRITE_CYCLE_TIME_MS 5

LOG_MODULE_REGISTER(eeprom, LOG_LEVEL_INF);

#define EEPROM_I2C_ADDR DT_REG_ADDR(DT_NODELABEL(eeprom))
static const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

static struct eeprom_data eeprom_data;

int eeprom_read(uint16_t addr, uint8_t *data)
{
	int ret;

	LOG_DBG("Starting 1 byte EEPROM read, addr=%#x", addr);

	ret = i2c_reg_read_byte(i2c_dev, EEPROM_I2C_ADDR | (addr >> 8),
				(uint8_t)addr, data);
	if (ret != 0) {
		DOLOS_LOG_ERR(ERROR_EEPROM, ret,
			      "Failed to read from EEPROM, addr=%#x, err=%d",
			      addr, ret);
		return ret;
	}

	LOG_DBG("Read from EEPROM, addr=%#x, data=%#x", addr, *data);
	return 0;
}

int eeprom_read_n(uint16_t addr, uint8_t *data, size_t len)
{
	int ret;
	uint8_t *data_ptr = data;
	size_t bytes_to_read = 0;
	size_t bytes_left = len;

	LOG_DBG("Starting %d byte EEPROM read, addr=%#x", bytes_left, addr);

	while (bytes_left > 0) {
		bytes_to_read =
			MIN(bytes_left, EEPROM_CHUNK_SIZE - (addr % 16));

		ret = i2c_write_read(i2c_dev, EEPROM_I2C_ADDR | (addr >> 8),
				     (uint8_t *)&addr, 1, data_ptr,
				     bytes_to_read);
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_EEPROM, ret,
				"Failed to read from EEPROM, addr=%#x, err=%d",
				addr, ret);
			return ret;
		}

		LOG_DBG("Read a chunk from EEPROM, addr=%#x, size=%d", addr,
			bytes_to_read);
		LOG_HEXDUMP_DBG(data_ptr, bytes_to_read, "Chunk: ");

		data_ptr += bytes_to_read;
		addr += bytes_to_read;
		bytes_left -= bytes_to_read;
	}

	LOG_DBG("Read from EEPROM, addr=%#x", addr);
	LOG_HEXDUMP_DBG(data, len, "Data read:");

	return 0;
}

int eeprom_write(uint16_t addr, uint8_t data)
{
	int ret;

	LOG_DBG("Starting EEPROM write, addr=%#x, data=%#x", addr, data);
	ret = i2c_reg_write_byte(i2c_dev, EEPROM_I2C_ADDR | (addr >> 8),
				 (uint8_t)addr, data);
	if (ret != 0) {
		DOLOS_LOG_ERR(
			ERROR_EEPROM, ret,
			"Failed to write to EEPROM, addr=%#x, data=%#x err=%d",
			addr, data, ret);
		return ret;
	}

	LOG_DBG("Wrote to EEPROM, addr=%#x, data=%#x", addr, data);

	return 0;
}

int eeprom_write_n(uint16_t addr, uint8_t *data, size_t len)
{
	int ret;
	uint8_t write_buf[EEPROM_PAGE_SIZE + 1];
	uint8_t *data_ptr = data;
	size_t bytes_to_write = 0;

	LOG_DBG("Starting EEPROM write, addr=%#x", addr);
	LOG_HEXDUMP_DBG(data, len, "Writing data:");

	while (len > 0) {
		bytes_to_write = MIN(len, EEPROM_PAGE_SIZE - (addr % 16));

		write_buf[0] = (uint8_t)addr;
		memcpy(&write_buf[1], data_ptr, bytes_to_write);

		ret = i2c_write(i2c_dev, &write_buf[0], bytes_to_write + 1,
				EEPROM_I2C_ADDR | (addr >> 8));
		if (ret != 0) {
			DOLOS_LOG_ERR(
				ERROR_EEPROM, ret,
				"Failed to write to EEPROM, addr=%#x, err=%d",
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

int eeprom_read_data(void)
{
	int ret;

	LOG_DBG("Reading all EEPROM data");

	ret = eeprom_read_n(0, (uint8_t *)&eeprom_data,
			    sizeof(struct eeprom_data));
	if (ret != 0) {
		DOLOS_LOG_ERR(ERROR_EEPROM, ret,
			      "Couldn't read EEPROM data, err=%d", ret);
		return ret;
	}

	return 0;
}

uint8_t eeprom_get_version(void)
{
	return eeprom_data.version;
}

uint8_t eeprom_get_sys_pres_pol(void)
{
	return eeprom_data.polarity;
}

uint8_t eeprom_get_year(void)
{
	return eeprom_data.manufactured_year;
}

uint8_t eeprom_get_week(void)
{
	return eeprom_data.manufactured_week;
}

uint16_t eeprom_get_serial_no(void)
{
	return eeprom_data.serial_number;
}

uint32_t eeprom_get_crc32(void)
{
	return eeprom_data.crc;
}

int eeprom_crc32_check(void)
{
	uint8_t *crc32_ver_data;
	uint32_t crc32;
	uint32_t calculated_crc32;

	LOG_DBG("Starting EEPROM CRC32 check");

	crc32 = eeprom_get_crc32();
	crc32_ver_data =
		((uint8_t *)&eeprom_data) + EEPROM_CRC32_VER_DATA_OFFSET;

	/* Bitwise NOT performed to obtain CRC32 JAMCRC */
	calculated_crc32 =
		~crc32_ieee(crc32_ver_data, EEPROM_CRC32_VER_DATA_SIZE);
	if (crc32 != calculated_crc32) {
		DOLOS_LOG_ERR(ERROR_EEPROM, -1,
			      "Incorrect CRC32, expected=0x%08X, got=0x%08X",
			      calculated_crc32, crc32);
		return -1;
	}

	LOG_DBG("CRC32 check succeeded");
	return 0;
}

struct sb_word_register *eeprom_get_word_reg(uint8_t offset)
{
	if (offset >= ARRAY_SIZE(eeprom_data.word_registers)) {
		return NULL;
	}

	return &eeprom_data.word_registers[offset];
}

struct sb_block_register *eeprom_get_block_reg(uint8_t offset)
{
	if (offset >= ARRAY_SIZE(eeprom_data.block_registers)) {
		return NULL;
	}

	return &eeprom_data.block_registers[offset];
}

struct sb_word_register *eeprom_get_word_reg_arr(size_t *arr_size)
{
	*arr_size = ARRAY_SIZE(eeprom_data.word_registers);
	return eeprom_data.word_registers;
}

struct sb_block_register *eeprom_get_block_reg_arr(size_t *arr_size)
{
	*arr_size = ARRAY_SIZE(eeprom_data.block_registers);
	return eeprom_data.block_registers;
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

	uint8_t data[EEPROM_SIZE];

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
	uint16_t addr;
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

	version = eeprom_get_version();
	sys_pres_pol = eeprom_get_sys_pres_pol();
	year = eeprom_get_year();
	week = eeprom_get_week();
	serial_no = eeprom_get_serial_no();
	crc32 = eeprom_get_crc32();

	shell_print(sh, "Version: %d", version);

	switch (sys_pres_pol) {
	case SYS_PRES_POL_LOW:
		shell_print(sh, "System present polarity: LOW");
		break;
	case SYS_PRES_POL_HIGH:
		shell_print(sh, "System present polarity: HIGH");
		break;
	case SYS_PRES_POL_FLOAT:
		shell_print(sh, "System present polarity: FLOAT");
		break;
	default:
		shell_print(sh, "System present polarity: UNKNOWN");
		break;
	}

	shell_print(sh, "Manufactured Year: %d", 2000 + year);
	shell_print(sh, "Manufactured Week: %d", week);
	shell_print(sh, "Serial number: 0x%04X", serial_no);
	shell_print(sh, "CRC32: 0x%08X", crc32);

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
