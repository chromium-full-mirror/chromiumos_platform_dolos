/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ERROR_H_
#define ERROR_H_

/* List of common error codes that can be returned */
enum dolos_error_list {
        /* Success - no error */
        DOLOS_SUCCESS = 0,
        /* Unknown error */
        DOLOS_ERROR_UNKNOWN = 1,
        /* Function not implemented yet */
        DOLOS_ERROR_UNIMPLEMENTED = 2,
        /* Overflow error; too much input provided. */
        DOLOS_ERROR_OVERFLOW = 3,
        /* Timeout */
        DOLOS_ERROR_TIMEOUT = 4,
        /* Already in use, or not ready yet */
        DOLOS_ERROR_BUSY = 5,
        /* Failed because component does not have power */
        DOLOS_ERROR_NOT_POWERED = 6,
        /* Failed because component is not calibrated */
        DOLOS_ERROR_NOT_CALIBRATED = 7,
        /* Failed because CRC error */
        DOLOS_ERROR_CRC = 8,
        /* Failed to read/write SMBus */
        DOLOS_ERROR_SMBUS = 9,
        /* Failed to read/write PAC */
        DOLOS_ERROR_PAC = 10,
        /* Failed to write to Flash */
        DOLOS_ERROR_FLASH = 11,
        /* Invalid argument */
        DOLOS_ERROR_INVALID = 11,
};

#endif /* ERROR_H_ */
