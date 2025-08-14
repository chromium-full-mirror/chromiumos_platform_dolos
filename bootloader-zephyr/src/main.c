/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

void print_uart(char *buf)
{
	while (*buf != 0)
	{
		uart_poll_out(uart_dev, *buf);
		buf++;
	}
}

void __attribute__((noreturn)) boot_main_image(void)
{
	const struct flash_area *app_partition;

	// 1. Find the application partition using the flash map API.
	if (flash_area_open(FIXED_PARTITION_ID(firmware_partition), &app_partition) != 0)
	{
		// If the main application partition isn't found, hang.
		while (1)
		{
			print_uart("FATAL: Main application partition not found!\n");
			// Sleep one second to avoid flooding console
			k_sleep(K_SECONDS(1));
		}
	}

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

int __attribute__((noreturn)) main(void)
{
	print_uart("Dolos bootloader startup\r\n");
	boot_main_image();
	__builtin_unreachable();
}
