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
#include "temperature.h"

#define BYTE 8

/* Defines for BATTERY_MODE register */
#define BATTERY_MODE_INTERNAL_CHARGE_CONTROLLER 0x0001
#define BATTERY_MODE_PRIMARY_BATTERY_SUPPORT 0x0002
#define BATTERY_MODE_CONDITION_FLAG 0x0080
#define BATTERY_MODE_CHARGE_CONTROLLER_ENABLED 0x0100
#define BATTERY_MODE_PRIMARY_BATTERY 0x0200
#define BATTERY_MODE_ALARM_MODE 0x2000
#define BATTERY_MODE_CHARGER_MODE 0x4000
#define BATTERY_MODE_CAPACITY_MODE 0x8000

#define DEF_READONLY_WORD_SB_REG(_address, ...)                                                    \
        {                                                                                          \
                .name = #_address, .address = SB_REG_##_address, .length = 2, .data = __VA_ARGS__, \
                .is_read_only = true, .is_word = true                                              \
        }

#define DEF_READONLY_BLOCK_SB_REG(_address, _length, ...)                                                \
        {                                                                                                \
                .name = #_address, .address = SB_REG_##_address, .length = _length, .data = __VA_ARGS__, \
                .is_read_only = true, .is_word = false                                                   \
        }

#define DEF_READWRITE_WORD_SB_REG(_address, ...)                                                   \
        {                                                                                          \
                .name = #_address, .address = SB_REG_##_address, .length = 2, .data = __VA_ARGS__, \
                .is_read_only = false, .is_word = true                                             \
        }

static struct sb_register sb_registers[] = {
        DEF_READWRITE_WORD_SB_REG(MANUFACTURER_ACCESS, { 0x83, 0x43 }),
        DEF_READWRITE_WORD_SB_REG(REMAINING_CAPACITY_ALARM, { 0x67, 0x02 }),
        DEF_READWRITE_WORD_SB_REG(REMAINING_TIME_ALARM, { 0x0a, 0x00 }),
        DEF_READWRITE_WORD_SB_REG(BATTERY_MODE, { 0x01, 0x60 }),
        DEF_READWRITE_WORD_SB_REG(AT_RATE, { 0x0, 0x0 }),

        DEF_READONLY_WORD_SB_REG(AT_RATE_TIME_TO_FULL, { 0xff, 0xff }),
        DEF_READONLY_WORD_SB_REG(AT_RATE_TIME_TO_EMPTY, { 0xff, 0xff }),
        DEF_READONLY_WORD_SB_REG(AT_RATE_OK, { 0x1, 0x0 }),
        DEF_READONLY_WORD_SB_REG(TEMPERATURE, { 0x24, 0x0c }),
        DEF_READONLY_WORD_SB_REG(VOLTAGE, { 0xd4, 0x20 }),
        DEF_READONLY_WORD_SB_REG(CURRENT, { 0x83, 0x0f }),
        DEF_READONLY_WORD_SB_REG(AVERAGE_CURRENT, { 0xf1, 0x09 }),
        DEF_READONLY_WORD_SB_REG(MAX_ERROR, { 0x01, 0x00 }),
        DEF_READONLY_WORD_SB_REG(RELATIVE_STATE_OF_CHARGE, { 0x63, 0x00 }),
        DEF_READONLY_WORD_SB_REG(ABSOLUTE_STATE_OF_CHARGE, { 0x58, 0x00 }),
        DEF_READONLY_WORD_SB_REG(REMAINING_CAPACITY, { 0x16, 0x11 }),
        DEF_READONLY_WORD_SB_REG(FULL_CHARGE_CAPACITY, { 0x2c, 0x15 }),
        DEF_READONLY_WORD_SB_REG(RUN_TIME_TO_EMPTY, { 0xff, 0xff }),
        DEF_READONLY_WORD_SB_REG(AVERAGE_TIME_TO_EMPTY, { 0xff, 0xff }),
        DEF_READONLY_WORD_SB_REG(AVERAGE_TIME_TO_FULL, { 0xff, 0xff }),

        DEF_READWRITE_WORD_SB_REG(CHARGING_CURRENT, { 0x0, 0x0 }),
        DEF_READWRITE_WORD_SB_REG(CHARGING_VOLTAGE, { 0x0, 0x0 }),

        DEF_READONLY_WORD_SB_REG(BATTERY_STATUS, { 0xe0, 0x00 }),
        DEF_READONLY_WORD_SB_REG(CYCLE_COUNT, { 0x21, 0x00 }),
        DEF_READONLY_WORD_SB_REG(DESIGN_CAPACITY, { 0x06, 0x18 }),
        DEF_READONLY_WORD_SB_REG(DESIGN_VOLTAGE, { 0x14, 0x1e }),
        DEF_READONLY_WORD_SB_REG(SPECIFICATION_INFO, { 0x31, 0x00 }),
        DEF_READONLY_WORD_SB_REG(MANUFACTURE_DATE, { 0x5b, 0x52 }),
        DEF_READONLY_WORD_SB_REG(SERIAL_NUMBER, { 0x9b, 0x00 }),

        DEF_READONLY_BLOCK_SB_REG(MANUFACTURER_NAME, 11,
                                  { 0x33, 0x33, 0x33, 0x2d, 0x41, 0x43, 0x2d, 0x44, 0x41, 0x2D, 0x41 }),
        DEF_READONLY_BLOCK_SB_REG(DEVICE_NAME, 9, { 0x47, 0x48, 0x30, 0x32, 0x30, 0x34, 0x37, 0x58, 0x4c }),
        DEF_READONLY_BLOCK_SB_REG(DEVICE_CHEMISTRY, 4, { 0x4c, 0x49, 0x4f, 0x4e }),
        DEF_READONLY_BLOCK_SB_REG(MANUFACTURER_DATA, 16,
                                  { 0x36, 0x4B, 0x51, 0x44, 0x43, 0x30, 0x41, 0x38, 0x31, 0x45, 0x56, 0x4B, 0x5A, 0x4A,
                                    0x00, 0x01 }),

        DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_5, { 0x00, 0x00 }),
        DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_4, { 0x00, 0x00 }),
        DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_3, { 0x00, 0x00 }),
        DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_2, { 0x10, 0x83 }),
        DEF_READONLY_WORD_SB_REG(OPTIONAL_MFG_FUNCTION_1, { 0x10, 0x84 }),
        DEF_READONLY_WORD_SB_REG(SB_PACK_STATUS, { 0x1, 0x00 }),

        DEF_READONLY_BLOCK_SB_REG(SB_ALT_MANUFACTURER_ACCESS, 6, { 0x54, 0x00, 0x87, 0x03, 0x00, 0x00 }),

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

/** Find smart_battery register in sb_registers array for given register address.
 *
 */
static int sb_read_word_register(enum sb_register_address address, uint16_t *result)
{
        struct sb_register *reg;

        reg = sb_find_register_index(address);
        if (reg == NULL) {
                return DOLOS_ERROR_OVERFLOW;
        }
        if (!reg->is_word) {
                return DOLOS_ERROR_INVALID;
        }

        *result = *((uint16_t *)&reg->data[0]);

        return DOLOS_SUCCESS;
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

int sb_write_register(enum sb_register_address address, uint8_t *buffer, size_t buf_size)
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
        uint16_t temp = temp_get_k() * 10;
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

bool sb_sbreg_command_handler(uint8_t address, char register_type, size_t length, uint8_t *data)
{
        /* Skip register if it is a reserved register */
        if (register_type == 'r' | register_type == 'R') {
                return true;
        }

        /* Write the data to the sb register */
        if (sb_write_register(address, data, length) != DOLOS_SUCCESS) {
                return false;
        }

        /* Write data to the flash */
        if (!sb_write_register_to_dflash(address)) {
                return false;
        }

        return true;
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

bool sb_is_register_read_only(enum sb_register_address address)
{
        struct sb_register *sb_register = sb_find_register_index(address);
        return sb_register->is_read_only;
}

bool sb_is_register_word(enum sb_register_address address)
{
        struct sb_register *sb_register = sb_find_register_index(address);
        return sb_register->is_word;
}

static void sb_print_sb_register_data(const struct sb_register *sb_register)
{
        printf("name=%-32s address=0x%02x length=%02d word=%d read_only=%d successful_read_count=%9d failed_read_count=%9d successful_read_count=%9d failed_write_count=%9d data=",
               sb_register->name, sb_register->address, sb_register->length, sb_register->is_word,
               sb_register->is_read_only, sb_register->successful_read_count, sb_register->failed_read_count,
               sb_register->successful_write_count, sb_register->failed_write_count);
        for (size_t i = 0; i < sb_register->length; i++) {
                printf("0x%02x ", sb_register->data[i]);
        }
        printf("\r\n");
}

void sb_print_sb_register(enum sb_register_address address)
{
        struct sb_register *sb_register = sb_find_register_index(address);
        if (sb_register == NULL) {
                printf("Register with given address was not found\r\n");
        }
        sb_print_sb_register_data(sb_register);
}

void sb_print_sb_registers(void)
{
        for (size_t i = 0; i < sizeof(sb_registers) / sizeof(sb_registers[0]); i++) {
                sb_print_sb_register_data(&sb_registers[i]);
        }
}

void sb_load_registers_from_dflash(void)
{
        for (int i = 0; i < sizeof(sb_registers) / sizeof(sb_registers[0]); i++) {
                sb_read_register_from_dflash(sb_registers[i].address);
        }
}

/* Reads the value of the BATTERY_MODE register */
static uint16_t sb_get_battery_mode(void)
{
        uint16_t battery_mode;
        int ret;

        ret = sb_read_word_register(SB_REG_BATTERY_MODE, &battery_mode);
        if (ret != DOLOS_SUCCESS) {
                return 0;
        }

        return battery_mode;
}

/* Reads the value of the DESIGN_VOLTAGE register */
static uint16_t sb_get_design_voltage(void)
{
        uint16_t design_voltage;
        int ret;

        ret = sb_read_word_register(SB_REG_DESIGN_VOLTAGE, &design_voltage);
        if (ret != DOLOS_SUCCESS) {
                return 0;
        }

        return design_voltage;
}

/* Reads the value of the DESIGN_CAPACITY register */
static uint16_t sb_get_design_capacity(void)
{
        uint16_t result;
        int ret;

        ret = sb_read_word_register(SB_REG_DESIGN_CAPACITY, &result);
        if (ret != DOLOS_SUCCESS) {
                return 0;
        }

        return result;
}

/* Sends the data of the DESIGN_CAPACITY register to the SMBus controller */
static int sb_design_capacity_read_handler(const struct sb_register *design_capacity_reg)
{
        uint16_t battery_mode;
        uint32_t design_capacity;

        battery_mode = sb_get_battery_mode();
        design_capacity = sb_get_design_capacity();
        /* Check CAPACITY MODE in BATTERY MODE register */
        if (battery_mode & BATTERY_MODE_CAPACITY_MODE) {
                /* Report in 10 mWh */
                uint32_t design_voltage = sb_get_design_voltage();
                uint32_t design_capacity_10mhw = design_capacity * design_voltage / 10000;
                dsb_target_write_word((uint8_t *)&design_capacity_10mhw);
        } else {
                /* Report in mAh */
                dsb_target_write_word(design_capacity_reg->data);
        }

        return 2;
}

int sb_register_smbus_read_handler(enum sb_register_address address)
{
        int read_bytes = 0;
        struct sb_register *sb_register = sb_find_register_index(address);
        if (sb_register == NULL) {
                return read_bytes;
        }

        if (address == SB_REG_DESIGN_CAPACITY) {
                read_bytes = sb_design_capacity_read_handler(sb_register);
        } else {
                if (sb_register->is_word) {
                        dsb_target_write_word(sb_register->data);
                } else {
                        dsb_target_write_block(sb_register->data, sb_register->length);
                }
                read_bytes = sb_register->length;
        }

        if (read_bytes != 0) {
                sb_register->successful_read_count++;
        } else {
                sb_register->failed_read_count++;
        }

        return read_bytes;
}

int sb_register_smbus_write_handler(enum sb_register_address address)
{
        int wrote_bytes = 0;
        uint8_t buffer[SB_REG_DATA_MAX_SIZE];
        uint8_t buf_size = 2;
        int ret;
        struct sb_register *sb_register;

        sb_register = sb_find_register_index(address);
        if (sb_register == NULL) {
                return wrote_bytes;
        }

        if (sb_register->is_word) {
                dsb_target_read_word(buffer);
        } else {
                dsb_target_read_block(buffer, &buf_size);
        }

        ret = sb_write_register(sb_register->address, buffer, buf_size);

        if (ret != 0) {
                sb_register->successful_write_count++;
        } else {
                sb_register->failed_write_count++;
        }

        return buf_size;
}
