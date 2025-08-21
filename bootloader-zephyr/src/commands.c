/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "commands.h"
#include "uart.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/storage/flash_map.h>

typedef void (*command_handler_t)(void);

struct command_entry {
	const char *name;
	command_handler_t handler;
        const char *desc;
};

static void __attribute__((noreturn)) command_boot(void);
static void __attribute__((noreturn)) command_bsl(void);
static void __attribute__((noreturn)) command_reset(void);
static void command_help(void);

/*
 * Command dispatch table.
 */
static const struct command_entry command_table[] = {
	{ "reset", command_reset, "reboots the device"},
	{ "bsl", command_bsl, "jumps to BSL"},
	{ "boot", command_boot, "boots main image"},
        { "help", command_help, "prints help message"}
};

void process_command(char *buf)
{
	for (int i = 0; i < ARRAY_SIZE(command_table); i++) {
		if (strcmp(buf, command_table[i].name) == 0) {
			command_table[i].handler();
			return;
		}
	}

	uart_print("Unrecognized command\r\n");
}

/*
 * Command handler functions
 */
static void command_help(void)
{
        uart_print("Available commands:\r\n");
        for (int i = 0; i < ARRAY_SIZE(command_table); i++) {
		uart_print(command_table[i].name);
                uart_print("\t-- ");
                uart_print(command_table[i].desc);
                uart_print("\r\n");
	}
}

 static void command_reset(void)
{
	uart_print("Resetting device...\r\n");
        k_sleep(K_MSEC(100));
	sys_reboot(SYS_REBOOT_COLD);
}

static void command_bsl(void)
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

static void command_boot(void)
{
        const struct flash_area *app_partition;

	// 1. Find the application partition using the flash map API.
	if (flash_area_open(FIXED_PARTITION_ID(firmware_partition), &app_partition) != 0)
	{
		// If the main application partition isn't found, hang.
		while (1)
		{
			uart_print("FATAL: Main application partition not found!\n");
			// Sleep one second to avoid flooding console
			k_sleep(K_SECONDS(1));
		}
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
