/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Reads 1 byte from Dolos Connector EEPROM
 *
 * @param addr EEPROM address to read data from
 * @param data Pointer to read data
 *
 * @retval 0 on success
 * @retval -EIO on input/output error.
 */
int eeprom_read(uint8_t addr, uint8_t *data);

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
int eeprom_read_n(uint8_t addr, uint8_t *data, size_t len);

/**
 * @brief Writes a single byte to Dolos Connector EEPROM
 *
 * @param addr EEPROM address to write data to
 * @param data Data to be written
 *
 * @retval 0 on success
 * @retval -EIO on input/output error.
 */
int eeprom_write(uint8_t addr, uint8_t data);

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
int eeprom_write_n(uint8_t addr, uint8_t *data, size_t len);

/**
 * @brief Reads the connecter EEPROM version.
 *
 * @param version Pointer to the EEPROM version read from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_version(uint8_t *version);

/**
 * @brief Reads the connecter EEPROM system present polarity.
 *
 * @param sys_pres_pol Pointer to the EEPROM system present polarity read from
 * EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_sys_pres_pol(uint8_t *sys_pres_pol);

/**
 * @brief Reads the connecter EEPROM manufacturer year.
 *
 * @param year Pointer to the EEPROM manufacturer year read from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_year(uint8_t *year);

/**
 * @brief Reads the connecter EEPROM manufacturer week.
 *
 * @param week Pointer to the EEPROM manufacturer week read from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_week(uint8_t *week);

/**
 * @brief Reads the connecter EEPROM serial number.
 *
 * @param serial_no Pointer to the EEPROM serial number read from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_serial_no(uint16_t *serial_no);

/**
 * @brief Reads the connecter EEPROM CRC32.
 *
 * @param crc32 Pointer to the EEPROM CRC32 read from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_crc32(uint32_t *crc32);

/**
 * @brief Reads a smart battery word register from the connecter EEPROM.
 *
 * @param offset The offset of the word register data in the EEPROM.
 * @param reg Pointer to the word register data read from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_word_reg(uint8_t offset, uint16_t *reg);

/**
 * @brief Reads a smart battery block register from the connecter EEPROM.
 *
 * @param offset The offset of the block register data in the EEPROM.
 * @param len Pointer to the length of the block register data.
 * @param reg Pointer to the block register data read from EEPROM.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_get_block_reg(uint8_t offset, uint8_t *len, uint8_t *reg);

/**
 * @brief Performs EEPROM CRC32 check. Reads the connector EEPROM CRC32 and
 * compares it against the CRC32 for the EEPROM battery info.
 *
 * @retval 0 on success.
 * @retval Negative on failure.
 */
int eeprom_crc32_check(void);

#endif /* EEPROM_H_ */
