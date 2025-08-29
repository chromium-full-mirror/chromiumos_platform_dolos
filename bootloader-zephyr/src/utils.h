/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

 #ifndef __UTILS_H__
 #define __UTILS_H__


/**
 * @brief Boots the main application image.
 *
 * Jumps to the main application's code in a separate flash partition.
 * @retval noreturn on success
 * @retval negative value on failure
 */
 int boot_main_image(void);

/**
 * @brief Jumps to BSL
 *
 * This function enters the BSL ROM bootloader and does not return.
 */
 void __attribute__((noreturn)) jump_to_bsl(void);

/**
 * @brief Initializes flags from flash.
 *
 * Reads flags from a read-only flash partition to configure bootloader behavior.
 * If the read fails, the flags are set to a default disabled state.
 */
 void init_flags(void);

/**
 * @brief Checks if the development flag is set.
 *
 * @retval 0 if flag is set
 * @retval non-zero value if flag is not set
 */
 int check_dev_flag_set(void);

 /**
  * @brief Checks if the stored CRC32 matches the computed CRC32 of the firmware
  *
  * @retval 0 if they match
  * @retval non-zero if different
  */
  int check_firmware_CRC(void);

 #endif /* __UTILS_H__ */
