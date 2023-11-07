/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "pac.h"
#include "dolos_smbus.h"
#include "log.h"
#include "error.h"
#include "smart_battery.h"
#include "dolos_flash.h"
#include "utils.h"
#include "ti_msp_dl_config.h"
#include "dolos_gpio.h"

#define BYTE 8

static struct sb_register sb_registers[] = {
        { SB_REG_MANUFACTURER_ACCESS, 2, { 0x83, 0x43 } },
        { SB_REG_REMAINING_CAPACITY_ALARM, 2, { 0x67, 0x02 } },
        { SB_REG_REMAINING_TIME_ALARM, 2, { 0x0a, 0x00 } },
        { SB_REG_BATTERY_MODE, 2, { 0x01, 0x60 } },
        { SB_REG_AT_RATE, 2, { 0x0, 0x0 } },
        { SB_REG_AT_RATE_TIME_TO_FULL, 2, { 0xff, 0xff } },
        { SB_REG_AT_RATE_TIME_TO_EMPTY, 2, { 0xff, 0xff } },
        { SB_REG_AT_RATE_OK, 2, { 0x01, 0x00 } },
        { SB_REG_TEMPERATURE, 2, { 0x24, 0x0c } },
        { SB_REG_VOLTAGE, 2, { 0xd4, 0x20 } },
        { SB_REG_CURRENT, 2, { 0x83, 0x0f } },
        { SB_REG_AVERAGE_CURRENT, 2, { 0xf1, 0x09 } },
        { SB_REG_MAX_ERROR, 2, { 0x01, 0x00 } },
        { SB_REG_RELATIVE_STATE_OF_CHARGE, 2, { 0x63, 0x00 } },
        { SB_REG_ABSOLUTE_STATE_OF_CHARGE, 2, { 0x58, 0x00 } },
        { SB_REG_REMAINING_CAPACITY, 2, { 0x16, 0x11 } },
        { SB_REG_FULL_CHARGE_CAPACITY, 2, { 0x2c, 0x15 } },
        { SB_REG_RUN_TIME_TO_EMPTY, 2, { 0xff, 0xff } },
        { SB_REG_AVERAGE_TIME_TO_EMPTY, 2, { 0xff, 0xff } },
        { SB_REG_AVERAGE_TIME_TO_FULL, 2, { 0xff, 0xff } },
        { SB_REG_CHARGING_CURRENT, 2, { 0x00, 0x00 } },
        { SB_REG_CHARGING_VOLTAGE, 2, { 0x00, 0x00 } },
        { SB_REG_BATTERY_STATUS, 2, { 0xe0, 0x00 } },
        { SB_REG_CYCLE_COUNT, 2, { 0x21, 0x00 } },
        { SB_REG_DESIGN_CAPACITY, 2, { 0x06, 0x18 } },
        { SB_REG_DESIGN_VOLTAGE, 2, { 0x14, 0x1e } },
        { SB_REG_SPECIFICATION_INFO, 2, { 0x31, 0x00 } },
        { SB_REG_MANUFACTURE_DATE, 2, { 0x5b, 0x52 } },
        { SB_REG_SERIAL_NUMBER, 2, { 0x9b, 0x00 } },
        { SB_REG_MANUFACTURER_NAME, 12, { 0x0b, 0x33, 0x33, 0x33, 0x2d, 0x41, 0x43, 0x2d, 0x44, 0x41, 0x2D, 0x41 } },
        { SB_REG_DEVICE_NAME, 0xa, { 0x09, 0x47, 0x48, 0x30, 0x32, 0x30, 0x34, 0x37, 0x58, 0x4c } },
        { SB_REG_DEVICE_CHEMISTRY, 5, { 0x04, 0x4c, 0x49, 0x4f, 0x4e } },
        { SB_REG_MANUFACTURER_DATA,
          0x11,
          { 0x10, 0x36, 0x4B, 0x51, 0x44, 0x43, 0x30, 0x41, 0x38, 0x31, 0x45, 0x56, 0x4B, 0x5A, 0x4A, 0x00, 0x01 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_5, 2, { 0x00 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_4, 2, { 0x00 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_3, 2, { 0x00 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_2, 2, { 0x10, 0x83 } },
        { SB_REG_OPTIONAL_MFG_FUNCTION_1, 2, { 0x10, 0x84 } },
        { SB_REG_SB_PACK_STATUS, 2, { 0x1 } },
        { SB_REG_SB_ALT_MANUFACTURER_ACCESS, 6, { 0x06, 0x54, 0x00, 0x87, 0x03, 0x00 } }

};

/** Find smart_battery register in sb_registers array for given register address.
 *
 */
static struct sb_register *sb_find_register_index(enum sb_register_address address)
{
        struct sb_register *reg;
        int i;

        if (address >= SB_REG_LAST) {
                return NULL;
        }

        for (i = 0; i < sizeof(sb_registers) / sizeof(sb_registers[0]); i++) {
                reg = &sb_registers[i];
                if (reg->address == address) {
                        return reg;
                }
        }
        return NULL;
}

/** Return a smart battery register value.
 *
 */
int sb_read_register(enum sb_register_address address, uint8_t *buffer, uint8_t *buf_size)
{
        struct sb_register *reg;

        reg = sb_find_register_index(address);
        if (reg == NULL) {
                return DOLOS_ERROR_OVERFLOW;
        }
        memcpy(buffer, reg->data, reg->length);
        *buf_size = reg->length;

        return DOLOS_SUCCESS;
}

static int sb_write_register(enum sb_register_address address, uint8_t *buffer, size_t buf_size)
{
        struct sb_register *reg;

        reg = sb_find_register_index(address);
        if (reg == NULL) {
                return DOLOS_ERROR_OVERFLOW;
        }
        memcpy(reg->data, buffer, buf_size);
        reg->length = buf_size;

        return DOLOS_SUCCESS;
}

void sb_update_registers(void)
{
        uint16_t voltage_mv = pac_read_voltage_mv();
        uint16_t current_ma = pac_read_current_ma();
        uint32_t temp = dgpio_get_temp_k() * 10;
        sb_write_register(SB_REG_VOLTAGE, (uint8_t *)&voltage_mv, sizeof(voltage_mv));
        sb_write_register(SB_REG_CURRENT, (uint8_t *)&current_ma, sizeof(current_ma));
        sb_write_register(SB_REG_TEMPERATURE, (uint8_t *)&temp, sizeof(temp));
}

bool sb_write_register_data_to_dflash(enum sb_register_address address, uint8_t *data, uint32_t length)
{
        DEBUG("Writing sb register data to flash: address = %#x", address);

        /* Load register address into MSB byte of key and offset = 0 into LSB of key to store the length */
        uint16_t key = address << BYTE;

        if (dflash_write_data(key, length) != DOLOS_SUCCESS) {
                DEBUG("Failed to write sb register data to flash");
                return false;
        }

        /* Load register address into MSB byte of key and offset = n into LSB of key to store the bytes
         * sb_register->data[2^n...2^n+3] */
        uint32_t data_segment;
        int32_t remaining_length = length;

        while (remaining_length > 0) {
                /* Offset += 1 */
                ++key;

                data_segment = 0;
                for (size_t i = 0; i < sizeof(uint32_t); i++) {
                        data_segment |= (uint32_t)(data[length - remaining_length] << (BYTE * i));
                        remaining_length--;
                        if (remaining_length <= 0) {
                                break;
                        }
                }

                if (dflash_write_data(key, data_segment) != 0) {
                        DEBUG("Failed to write sb register data to flash");
                        return false;
                }
        }

        return true;
}

bool sb_write_register_to_dflash(enum sb_register_address address)
{
        DEBUG("Writing sb register to flash: register = %#x", address);

        /* Check if register address is correct */
        struct sb_register *sb_register = sb_find_register_index(address);
        if (sb_register == NULL) {
                DEBUG("Failed to find register with address %#x", address);
                return false;
        }

        return sb_write_register_data_to_dflash(address, sb_register->data, sb_register->length);
}

bool sb_read_register_data_from_dflash(enum sb_register_address address, uint8_t *data, uint32_t *length)
{
        DEBUG("Reading sb register data from flash: address = %#x", address);

        /* Load register address into MSB byte of key and offset = 0 into LSB of key to read the length */
        uint16_t key = address << BYTE;

        if (!dflash_read_data(key, length)) {
                DEBUG("Failed to read sb register data from flash");
                return false;
        }

        /* Load register address into MSB byte of key and offset = n into LSB of key to read the bytes
         * sb_register->data[2^n...2^n+3] */
        uint32_t data_segment;
        uint32_t remaining_length = 0;

        while (remaining_length < *length) {
                /* Offset += 1 */
                ++key;

                if (!dflash_read_data(key, &data_segment)) {
                        DEBUG("Failed to read sb register data from flash");
                        return false;
                }

                for (size_t i = 0; i < sizeof(uint32_t); i++) {
                        data[remaining_length + i] = (uint8_t)(data_segment >> (BYTE * i));
                }

                remaining_length += sizeof(uint32_t);
        }

        return true;
}

bool sb_read_register_from_dflash(enum sb_register_address address)
{
        /* Check if register address is correct */
        struct sb_register *sb_register = sb_find_register_index(address);
        if (sb_register == NULL) {
                DEBUG("Failed to find register with address %#x", address);
                return false;
        }

        /* Read the data from the flash */
        uint32_t length;
        if (sb_read_register_data_from_dflash(address, sb_register->data, &length)) {
                sb_register->length = length;
                return true;
        }

        return false;
}

void sb_sbreg_command_handler(uint8_t *cmd_data)
{
        DEBUG("Handling sbreg command: cmd data = %s", cmd_data);
        uint8_t address;
        char register_type;
        size_t length;
        uint8_t *data;

        enum parse_status parse_status;
        char *token;

        /* Read address byte */
        token = strtok((char *)cmd_data, " ");
        parse_status = parse_int(token, (int *)&address);
        if (parse_status != PARSE_SUCCESS) {
                DEBUG("sbreg invalid syntax");
                return;
        }

        /* Read command type, skip if  type = 'R' | 'r' (Reserved) */
        token = strtok(NULL, " ");
        if (!strcmp(token, "R") | !strcmp(token, "r")) {
                DEBUG("Skipping register %#x (Reserved)", address);
                return;
        }

        /* Read command length */
        token = strtok(NULL, " ");
        parse_status = parse_int(token, (int *)&length);
        if (parse_status != PARSE_SUCCESS) {
                DEBUG("sbreg invalid syntax");
                return;
        }

        /* Read the rest of data */
        data = malloc(sizeof(uint8_t) * length);
        size_t i = 0;
        uint8_t current_byte;

        token = strtok(NULL, " ");

        while (token != NULL && i < length) {
                parse_status = parse_int(token, (int *)&current_byte);
                if (parse_status != PARSE_SUCCESS) {
                        DEBUG("sbreg invalid syntax");
                        return;
                }
                data[i++] = current_byte;
                token = strtok(NULL, " ");
        }

        /* If there's data left, fail the command*/
        if (token != NULL || i < length) {
                free(data);
                DEBUG("sbreg invalid syntax");
                return;
        }

        /* Write the data to the sb register */
        sb_write_register(address, data, length);

        /* Write data to the flash */
        sb_write_register_to_dflash(address);

        free(data);
        printf("\n");
}

void sb_print_register_from_dflash(uint8_t *cmd_data)
{
        DEBUG("Handling print-register-from-flash command: cmd data = %s", cmd_data);

        uint8_t address;
        uint32_t length;
        uint8_t data[SB_REG_DATA_MAX_SIZE];
        enum parse_status parse_status;
        char *token;

        /* Read the register number and read it from flash */
        token = strtok((char *)cmd_data, " ");
        parse_status = parse_int(token, (int *)&address);
        if (parse_status != PARSE_SUCCESS) {
                printf("print-register-from-flash invalid syntax\r\n");
                return;
        }

        if (!sb_read_register_data_from_dflash(address, data, &length)) {
                printf("Failed to read register data from flash\r\n");
                return;
        }

        printf("Smart Battery Register %#x in flash:\r\n", address);
        printf("   Data Length: %d\r\n", length);
        printf("   Data:");
        for (size_t i = 0; i < length; i++) {
                printf(" %02x", data[i]);
        }
        printf("\r\n");
}
