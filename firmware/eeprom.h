/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include <stdbool.h>

#define EEPROM_MAX_SIZE 256

#define EEPROM_SERIAL_SIZE 20
#define EEPROM_SERIAL_FORMAT "DOLOSV1-C-YYMMDDXXXX"

/* Reads all bytes of EEPROM, returns DOLOS_SUCCESS on success */
int eeprom_read_all(uint8_t *data);

/* Reads byte from a specific address of EEPROM, returns DOLOS_SUCCESS on success */
int eeprom_read(uint8_t address, uint8_t *data);

/* Writes byte at a specific address of EEPROM, returns DOLOS_SUCCESS on success */
int eeprom_write(uint8_t address, uint8_t data);

/* Reads serial number from EEPROM */
int eeprom_serial_read(char *serial);

/* Writes serial number to EEPROM */
int eeprom_serial_write(char *serial);

/* Validates if serial is correct */
bool eeprom_serial_validate(char *serial);

#endif /* EEPROM_H_ */
