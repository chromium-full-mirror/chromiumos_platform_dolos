/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ERROR_H_
#define ERROR_H_

#include <stddef.h>
#include <stdint.h>

#define MAX_ERRORS 10
extern char eeprom_status[32];

/* Helper macro to log errors */
#define DOLOS_LOG_ERR(src, err_code, msg, ...)                                \
	do {                                                                  \
		error_log(src, __LINE__, err_code);                           \
		LOG_ERR(msg, ##__VA_ARGS__);                                  \
		if (src == ERROR_EEPROM) {                                    \
			if (strstr(msg, "Incorrect CRC32") != NULL) {         \
				strcpy(eeprom_status, "CRC Failure.");        \
			} else if (strstr(msg, "Failed to write") != NULL) {  \
				strcpy(eeprom_status, "EEPROM Write Error."); \
			} else if ((strstr(msg, "Failed to read") != NULL) || \
				   (strstr(msg, "Couldn't read EEPROM") !=    \
				    NULL)) {                                  \
				strcpy(eeprom_status, "EEPROM Read Error.");  \
			} else {                                              \
				strcpy(eeprom_status, "Undefined Error.");    \
			}                                                     \
		}                                                             \
	} while (0)

enum error_source {
	ERROR_EEPROM = 0,
	ERROR_SMART_BATTERY,
	ERROR_BMS,
	ERROR_SMBUS_TARGET,
	ERROR_I2C,
	ERROR_PAC,
	ERROR_BUCKBOOST,
	ERROR_TEMPERATURE,
	ERROR_UNKNOWN,
};

struct error_info {
	/* Uptime when the error occurred */
	uint64_t timestamp;
	/* Module that raised the error */
	enum error_source source;
	/* Line that raised the error */
	uint32_t line_number;
	/* Error code raised */
	int error_code;
	/*Error counter*/
	int occurrences;
};

/**
 * @brief Logs an error to the error module
 */
void error_log(enum error_source source, uint32_t line_number, int error_code);

/**
 * @brief Get all logged errors
 *
 * @param error_list A pointer to the logged error list
 *
 * @return Returns the number of currently logged errors
 */
size_t error_list(struct error_info error_list[MAX_ERRORS]);

/**
 * @brief Clears all errors
 */
void error_clear(void);

#endif /* ERROR_H_ */
