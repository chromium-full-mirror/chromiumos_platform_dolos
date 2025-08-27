/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "utils.h"
#include "uart.h"
#include <string.h>
#include <zephyr/storage/flash_map.h>

#define RO_RECORD_SIZE 32
#define DEV_FLAG_OFFSET 0
#define DEV_FLAG_MASK 0x1

static char flags[RO_RECORD_SIZE];

enum ro_record_offset {
        RO_RECORD_OFFSET_VERSION = 0,
        RO_RECORD_OFFSET_FLAGS = RO_RECORD_SIZE,
        RO_RECORD_OFFSET_UNUSED_1 = 2 * RO_RECORD_SIZE,
        RO_RECORD_OFFSET_UNUSED_2 = 3 * RO_RECORD_SIZE
};

void init_flags(void)
{
        const struct flash_area *ro_partition;
        if (flash_area_open(FIXED_PARTITION_ID(ro_partition), &ro_partition) != 0)
        {
                uart_print("ERROR: Unable to open RO partition. ");
                uart_print("Proceeding with default settings\r\n");
                return;
        }

        // Read flags
        if(flash_area_read(ro_partition, RO_RECORD_OFFSET_FLAGS, flags, RO_RECORD_SIZE) != 0)
        {
                uart_print("ERROR: Unable to read flags. ");
                uart_print("Proceeding with default settings\r\n");
                // Init flags to flash erase value which is treated as disabled
                memset(flags, 0xff, RO_RECORD_SIZE);
        }
}

int check_dev_flag_set(void) {
        return (uint8_t) DEV_FLAG_MASK & flags[DEV_FLAG_OFFSET];
}

int boot_main_image(void)
{
        const struct flash_area *app_partition;

        // 1. Find the application partition using the flash map API.
        if (flash_area_open(FIXED_PARTITION_ID(firmware_partition), &app_partition) != 0)
        {
                uart_print("FATAL: Main application partition not found!\n");
                return -1;
        }

        uart_print("Booting main image...\r\n");
        k_sleep(K_MSEC(100));

        uint32_t app_code_start_addr = app_partition->fa_off;
        flash_area_close(app_partition);

        // 2. Disable interrupt response.
        __disable_irq();

        // 3. Disable all enabled interrupts in NVIC.
        memset((uint32_t *)NVIC->ICER, 0xFF, sizeof(NVIC->ICER));

        // 4. Clear all pending interrupt requests in NVIC.
        memset((uint32_t *)NVIC->ICPR, 0xFF, sizeof(NVIC->ICPR));

        // 5. Disable SysTick and clear its exception pending bit.
        SysTick->CTRL = 0;
        SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

        /* 6. Load the vector table address of user application code in to VTOR.
                *     The application's vector table is at its start address.
                */
        SCB->VTOR = app_code_start_addr;

        /* 7. Use the MSP as the current SP.
                * Set the MSP with the value from the vector table used by the application.
                */
        __set_MSP(((uint32_t *)(SCB->VTOR))[0]);

        // In thread mode, enable privileged access and use the MSP as the current SP.
        __set_CONTROL(0);

        // 8. Enable interrupts.
        __enable_irq();

        // 9. Jump to the application's reset handler
        uint32_t reset_handler_address = ((uint32_t *)(SCB->VTOR))[1];
        void (*app_reset_handler)(void) = (void (*)(void))reset_handler_address;
        app_reset_handler();

        // Not reaching this point
        __builtin_unreachable();
}

void jump_to_bsl(void)
{
        uart_print("Jumping to BSL...\r\n");
	k_sleep(K_MSEC(100));
	__asm(
	#if defined(__GNUC__)
		".syntax unified\n"
	#endif
		"ldr     r4, = 0x41C40018\n"
		"ldr     r4, [r4]\n"
		"ldr     r1, = 0x03FF0000\n"
		"ands    r4, r1\n"
		"lsrs    r4, r4, #6\n"
		"ldr     r1, = 0x20300000\n"
		"adds    r2, r4, r1\n"
		"movs    r3, #0\n"
		"init_ecc_loop: \n"
		"str     r3, [r1]\n"
		"adds    r1, r1, #4\n"
		"cmp     r1, r2\n"
		"blo     init_ecc_loop\n"
		"ldr     r1, = 0x20200000\n"
		"adds    r2, r4, r1\n"
		"movs    r3, #0\n"
		"init_data_loop:\n"
		"str     r3, [r1]\n"
		"adds    r1, r1, #4\n"
		"cmp     r1, r2\n"
		"blo     init_data_loop\n"
		"str     %[resetLvlVal], [%[resetLvlAddr], #0x00]\n"
		"str     %[resetCmdVal], [%[resetCmdAddr], #0x00]"
		: /* No outputs */
		: [resetLvlAddr] "r"(&SYSCTL->SOCLOCK.RESETLEVEL),
		[resetLvlVal] "r"(DL_SYSCTL_RESET_BOOTLOADER_ENTRY),
		[resetCmdAddr] "r"(&SYSCTL->SOCLOCK.RESETCMD),
		[resetCmdVal] "r"(SYSCTL_RESETCMD_KEY_VALUE | SYSCTL_RESETCMD_GO_TRUE)
		: "r1", "r2", "r3", "r4");

	__builtin_unreachable();
}
