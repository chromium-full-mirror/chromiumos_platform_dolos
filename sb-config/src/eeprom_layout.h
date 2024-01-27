/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SB_CONFIG_EEPROM_LAYOUT_H
#define SB_CONFIG_EEPROM_LAYOUT_H

#include <stdint.h>

#define SB_BLOCK_REGISTER_MAX_LENGTH 32

struct sb_block_register {
  uint8_t length;
  uint8_t data[SB_BLOCK_REGISTER_MAX_LENGTH];
} __attribute__((packed));

struct eeprom_data {
  uint8_t version;
  uint8_t polarity;
  uint8_t manufactured_year;
  uint8_t manufactured_month;
  uint16_t serial_number;
  uint8_t reserved[9];
  uint32_t crc;
  struct sb_block_register block_registers[5];
  uint16_t word_register[36];
} __attribute__((packed));

#define STATIC_ASSERT_STRUCT_SIZE(struct_name, size)                           \
  typedef char assert_struct_##struct_name##_size                              \
      [(sizeof(struct struct_name) == size) ? 1 : -1]

STATIC_ASSERT_STRUCT_SIZE(sb_block_register, 33);
STATIC_ASSERT_STRUCT_SIZE(eeprom_data, 256);

#endif // SB_CONFIG_EEPROM_LAYOUT_H
