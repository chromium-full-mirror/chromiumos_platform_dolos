/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum eeprom_sys_pres_pol {
	SYS_PRES_POL_LOW = 0,
	SYS_PRES_POL_HIGH = 1,
	SYS_PRES_POL_FLOAT = 2,
};

/**
 * @brief Reads 1 byte from Dolos Connector EEPROM
 *
 * @param addr EEPROM address to read data from
 * @param data Pointer to read data
 *
 * @retval 0 on success
 * @retval -EIO on input/output error.
 */
int eeprom_read(uint16_t addr, uint8_t *data);

/**
 * @brief Reads N bytes from Dolos Connector EEPROM
 *
 * @param addr EEPROM address to read data from
 * @param data Pointer to read data
 * @param len Number of bytes to be read
 *
 * @retval 0 on success
 * @retval -EIO on input/output error.
 */
int eeprom_read_n(uint16_t addr, uint8_t *data, size_t len);

/**
 * @brief Writes a single byte to Dolos Connector EEPROM
 *
 * @param addr EEPROM address to write data to
 * @param data Data to be written
 *
 * @retval 0 on success
 * @retval -EIO on input/output error.
 */
int eeprom_write(uint16_t addr, uint8_t data);

/**
 * @brief Writes N bytes to Dolos Connector EEPROM
 *
 * @param addr EEPROM address to write data to
 * @param data Point to data to be written
 * @param len Number of bytes to be written
 *
 * @retval 0 on success
 * @retval -EIO on input/output error.
 */
int eeprom_write_n(uint16_t addr, uint8_t *data, size_t len);

/**
 * @brief Reads all of the EEPROM data. Should be called before getting any data
 * from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_read_data(void);

/**
 * @brief Reads the connecter EEPROM version.
 *
 * @return Version
 */
uint8_t eeprom_get_version(void);

/**
 * @brief Reads the connecter EEPROM system present polarity.
 *
 * @return System present polarity.
 */
uint8_t eeprom_get_sys_pres_pol(void);

/**
 * @brief Reads the connecter EEPROM manufacturer year.
 *
 * @return Manufacturer year.
 */
uint8_t eeprom_get_year(void);

/**
 * @brief Reads the connecter EEPROM manufacturer week.
 *
 * @return Manufacturer week.
 */
uint8_t eeprom_get_week(void);

/**
 * @brief Reads the connecter EEPROM serial number.
 *
 * @return Serial number
 */
uint16_t eeprom_get_serial_no(void);

/**
 * @brief Reads the connecter EEPROM CRC32.
 *
 * @return CRC32.
 */
uint32_t eeprom_get_crc32(void);

/**
 * @brief Performs EEPROM CRC32 check. Reads the connector EEPROM CRC32 and
 * compares it against the CRC32 for the EEPROM battery info.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_crc32_check(void);

/**
 * @brief Get a word register from the eeprom_data struct.
 *
 * @param offset The offset of the word register data in the eeprom_data struct.
 *
 * @return Returns a pointer to sb_word_register struct if register offset is
 * correct, NULL otherwise.
 */
struct sb_word_register *eeprom_get_word_reg(uint8_t offset);

/**
 * @brief Get a block register from the eeprom_data struct.
 *
 * @param offset The offset of the block register data in the eeprom_data
 * struct.
 *
 * @return Returns a pointer to sb_block_register struct if register offset is
 * correct, NULL otherwise.
 */
struct sb_block_register *eeprom_get_block_reg(uint8_t offset);

/**
 * @brief Get word registers array from the eeprom_data struct.
 *
 * @param arr_size Pointer for the array size;
 *
 * @return Returns a pointer to sb_word_registers array.
 */
struct sb_word_register *eeprom_get_word_reg_arr(size_t *arr_size);

/**
 * @brief Get block registers array from the eeprom_data struct.
 *
 * @param arr_size Pointer for the array size;
 *
 * @return Returns a pointer to sb_block_registers array.
 */
struct sb_block_register *eeprom_get_block_reg_arr(size_t *arr_size);

#endif /* EEPROM_H_ */
