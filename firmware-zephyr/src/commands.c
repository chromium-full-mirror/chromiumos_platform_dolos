/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "app_version.h"
#include "bms.h"
#include "eeprom.h"
#include "error.h"
#include "led.h"
#include "smbus_target.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

#include <ti/driverlib/m0p/dl_sysctl.h>
/**
 * Handles the command that jumps from main code to BSL.
 */
static int cmd_jump_to_bsl_handler(const struct shell *sh, size_t argc,
				   char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Going into BSL mode");

	/* Erase SRAM completely before jumping to BSL */
	__asm(
#if defined(__GNUC__)
		".syntax unified\n" /* Load SRAMFLASH register*/
#endif
		"ldr     r4, = 0x41C40018\n" /* Load SRAMFLASH register*/
		"ldr     r4, [r4]\n"
		"ldr     r1, = 0x03FF0000\n" /* SRAMFLASH.SRAM_SZ mask */
		"ands    r4, r1\n" /* Get SRAMFLASH.SRAM_SZ */
		"lsrs    r4, r4, #6\n" /* SRAMFLASH.SRAM_SZ to kB */
		"ldr     r1, = 0x20300000\n" /* Start of ECC-code */
		"adds    r2, r4, r1\n" /* End of ECC-code */
		"movs    r3, #0\n"
		"init_ecc_loop: \n" /* Loop to clear ECC-code */
		"str     r3, [r1]\n"
		"adds    r1, r1, #4\n"
		"cmp     r1, r2\n"
		"blo     init_ecc_loop\n"
		"ldr     r1, = 0x20200000\n" /* Start of NON-ECC-data */
		"adds    r2, r4, r1\n" /* End of NON-ECC-data */
		"movs    r3, #0\n"
		"init_data_loop:\n" /* Loop to clear ECC-data */
		"str     r3, [r1]\n"
		"adds    r1, r1, #4\n"
		"cmp     r1, r2\n"
		"blo     init_data_loop\n"
		/* Force a reset calling BSL after clearing SRAM */
		"str     %[resetLvlVal], [%[resetLvlAddr], #0x00]\n"
		"str     %[resetCmdVal], [%[resetCmdAddr], #0x00]"
		: /* No outputs */
		: [resetLvlAddr] "r"(&SYSCTL->SOCLOCK.RESETLEVEL),
		  [resetLvlVal] "r"(DL_SYSCTL_RESET_BOOTLOADER_ENTRY),
		  [resetCmdAddr] "r"(&SYSCTL->SOCLOCK.RESETCMD),
		  [resetCmdVal] "r"(SYSCTL_RESETCMD_KEY_VALUE |
				    SYSCTL_RESETCMD_GO_TRUE)
		: "r1", "r2", "r3", "r4");
	return 0;
}

/**
 * Handles the version command which prints the Dolos firmware version
 */
static int cmd_version_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Dolos version %s", APP_VERSION_STRING);

	return 0;
}

static bool read_gpio_state(const struct shell *sh,
			    const struct gpio_dt_spec *spec)
{
	int value = gpio_pin_get(spec->port, spec->pin);
	if (value < 0) {
		shell_print(sh, "Error reading GPIO pin\n");
		return -1;
	}

	return (value != 0);
}

void print_system_present_state(const struct shell *sh)
{
	/* Get system present force status string */
	char sys_pres_forced_str[64] = { 0 };
	sprintf(sys_pres_forced_str, " (Signal was forced, raw signal %s)",
		bms_get_sys_pres_raw() ? "HIGH" : "LOW");

	shell_print(sh,
		    "SYSTEM_PRESENT pin state : %s (signal %s, polarity %s%s)\n",
		    bms_get_sys_pres() ? "Present" : "Absent",
		    bms_get_sys_pres() ? "HIGH" : "LOW",
		    bms_get_sys_pres_pol() ? "HIGH" : "LOW",
		    bms_is_sys_pres_forced() ? sys_pres_forced_str : "");
}

int print_leds_state(const struct shell *sh)
{
	shell_print(sh, "Error LED                : %s\r\n",
		        dled_error_led_is_on() ? "On" : "Off");
	shell_print(sh, "Programmed LED           : %s\r\n",
		        dled_program_led_is_on() ? "On" : "Off");
	shell_print(sh, "Ready LED                : %s\r\n",
		        dled_ready_led_is_on() ? "On" : "Off");
	shell_print(sh, "Powering LED             : %s\r\n",
		        dled_powering_led_is_on() ? "On" : "Off");
	return 0;
}

// Define a command handler function for GPIO pins
static int gpio_cmd_handler(const struct shell *sh, size_t argc, char **argv)
{
	bool state;
	if (argc == 1) {
		const struct gpio_dt_spec spec_efuse_pg =
			GPIO_DT_SPEC_GET(DT_NODELABEL(efuse_pg), gpios);
		state = read_gpio_state(sh, &spec_efuse_pg);
		shell_print(sh, "EFSUE_PG pin state       : %d\r\n", state);
		const struct gpio_dt_spec spec =
			GPIO_DT_SPEC_GET(DT_NODELABEL(bq_charge_ok), gpios);
		state = read_gpio_state(sh, &spec);
		shell_print(sh, "CHRG_OK pin state        : %d\r\n", state);
		shell_print(sh, "Power Output Enable      : %d\r\n",
			    bms_get_power_output_state());
		print_system_present_state(sh);
		print_leds_state(sh);
	}
	return 0;
}

// Define a command handler function for GPIO sub-command
static int sub_gpio_cmd_handler(const struct shell *sh, size_t argc,
				char **argv, void *data)
{
	bool state;
	if (strcmp("efuse-pg", (char *)data) == 0) {
		const struct gpio_dt_spec spec =
			GPIO_DT_SPEC_GET(DT_NODELABEL(efuse_pg), gpios);
		state = read_gpio_state(sh, &spec);
		shell_print(sh, "EFSUE_PG pin state: %d\r\n", state);
	} else if (strcmp("chrg-ok", (char *)data) == 0) {
		const struct gpio_dt_spec spec =
			GPIO_DT_SPEC_GET(DT_NODELABEL(bq_charge_ok), gpios);
		state = read_gpio_state(sh, &spec);
		shell_print(sh, "CHRG_OK pin state: %d\r\n", state);
	} else if (strcmp("system-present", (char *)data) == 0) {
		print_system_present_state(sh);
	}

	return 0;
}

static char how_jump_to[4][140] = {
	"Transition to BMS_STATE_POWER_OUTPUT_OFF happens when either the system or charger is disconnected.",
	"Transition to BMS_STATE_TIMER happens when the system and charger are connected.",
	"Transition to BMS_STATE_POWER_GOOD_WAIT happens when the system and charger are connected and the input voltage is out of efuse range.",
	"Transition to BMS_STATE_POWER_OUTPUT_ON happens when the system and charger are connected and the input voltage is within efuse range."
};

static int status_cmd_handler(const struct shell *sh, int argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* Calculate uptime */
	uint64_t uptime = k_uptime_get();
	uint32_t days;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;

	days = uptime / (1000 * 60 * 60 * 24);
	uptime %= 1000 * 60 * 60 * 24;
	hours = uptime / (1000 * 60 * 60);
	uptime %= 1000 * 60 * 60;
	minutes = uptime / (1000 * 60);
	uptime %= 1000 * 60;
	seconds = uptime / 1000;

	/* Get system present force status string */
	char sys_pres_forced_str[64] = { 0 };
	sprintf(sys_pres_forced_str, " (Signal was forced, raw signal %s)",
		bms_get_sys_pres_raw() ? "HIGH" : "LOW");

	/* Get BMS current state string */
	enum bms_state bms_state = bms_get_curr_state();
	char bms_state_str[32] = { 0 };
	char next_bms_state_str[128] = { 0 };
	char next_bms_state_desc[512] = { 0 };

	switch (bms_state) {
	case BMS_STATE_POWER_OUTPUT_OFF:
		strcpy(bms_state_str, ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
		strcpy(next_bms_state_str, ENUM_NAME(BMS_STATE_TIMER));
		sprintf(next_bms_state_desc, "%s",
			how_jump_to[BMS_STATE_TIMER]);
		break;
	case BMS_STATE_TIMER:
		strcpy(bms_state_str, ENUM_NAME(BMS_STATE_TIMER));
		sprintf(next_bms_state_str, "Next States can be %s or %s or %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF),
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_ON),
			ENUM_NAME(BMS_STATE_POWER_GOOD_WAIT));
		sprintf(next_bms_state_desc, "%s\n%24s %s\n%24s %s",
			how_jump_to[BMS_STATE_POWER_OUTPUT_OFF], "",
			how_jump_to[BMS_STATE_POWER_OUTPUT_ON], "",
			how_jump_to[BMS_STATE_POWER_GOOD_WAIT]);
		break;
	case BMS_STATE_POWER_GOOD_WAIT:
		strcpy(bms_state_str, ENUM_NAME(BMS_STATE_POWER_GOOD_WAIT));
		sprintf(next_bms_state_str, "Next States can be %s or %s",
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_ON),
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
		sprintf(next_bms_state_desc, "%s\n%24s %s",
			how_jump_to[BMS_STATE_POWER_OUTPUT_ON], "",
			how_jump_to[BMS_STATE_POWER_OUTPUT_OFF]);
		break;
	case BMS_STATE_POWER_OUTPUT_ON:
		strcpy(bms_state_str, ENUM_NAME(BMS_STATE_POWER_OUTPUT_ON));
		sprintf(next_bms_state_str, "Next States can be %s or %s",
			ENUM_NAME(BMS_STATE_POWER_GOOD_WAIT),
			ENUM_NAME(BMS_STATE_POWER_OUTPUT_OFF));
		sprintf(next_bms_state_desc, "%s\n%24s %s",
			how_jump_to[BMS_STATE_POWER_GOOD_WAIT], "",
			how_jump_to[BMS_STATE_POWER_OUTPUT_OFF]);
		break;
	default:
		strcpy(bms_state_str, ENUM_NAME(BMS_STATE_INVALID));
		strcpy(next_bms_state_str, ENUM_NAME(BMS_STATE_INVALID));
		break;
	}

	/* Print status */
	shell_print(sh, "Dolos status:");
	shell_print(
		sh,
		"    Uptime             : %d days and %02d:%02d:%02d seconds",
		days, hours, minutes, seconds);
	shell_print(sh, "    Charger            : %s",
		    bms_get_chrg_ok() ? "Detected" : "Not detected");
	shell_print(sh, "    E-Fuse Power       : %s",
		    bms_get_efuse_pg() ? "Good" : "Not good");
	shell_print(sh,
		    "    System present     : %s (signal %s, polarity %s%s)",
		    bms_get_sys_pres() ? "Present" : "Absent",
		    bms_get_sys_pres_raw() ? "HIGH" : "LOW",
		    bms_get_sys_pres_pol() ? "HIGH" : "LOW",
		    bms_is_sys_pres_forced() ? sys_pres_forced_str : "");
	shell_print(sh, "    BMS current state  : %s", bms_state_str);
	shell_print(sh, "    BMS next state     : %s", next_bms_state_str);
	if (strcmp(next_bms_state_str, ENUM_NAME(BMS_STATE_INVALID)) != 0) {
		shell_print(sh, "%24s %s", "", next_bms_state_desc);
	}
	shell_print(sh, "    SMBus communication: %s",
		    smbus_target_is_communication_detected() ? "Detected" :
							       "Not detected");
	if (strcmp(eeprom_status, "Successful") == 0) {
		shell_print(sh,
			    "    Serial number      : DOLOSV%d-C-%02d20%02d%04d",
			    eeprom_get_version(), eeprom_get_week(),
			    eeprom_get_year(), eeprom_get_serial_no());
	} else {
		shell_print(
			sh,
			"    Serial number      : No cable - can't read serial.");
	}
	shell_print(sh, "    EEPROM             : %s", eeprom_status);

	return 0;
}

static int cmd_reset_handler(const struct shell *shell, size_t argc,
			     char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Resetting device...\n");

	// Delay to ensure the message is printed
	k_sleep(K_SECONDS(1));

	sys_reboot(SYS_REBOOT_COLD);

	return 0;
}

SHELL_CMD_REGISTER(reset, NULL, "Reset the device", cmd_reset_handler);
SHELL_CMD_REGISTER(version, NULL, "Prints Dolos version", cmd_version_handler);
SHELL_CMD_REGISTER(bsl, NULL, "Invokes BSL", cmd_jump_to_bsl_handler);

SHELL_SUBCMD_DICT_SET_CREATE(sub_gpio_cmds, sub_gpio_cmd_handler,
			     (efuse_pg, "efuse-pg", "efuse-pg value"),
			     (chrg_ok, "chrg-ok", "chrg-ok value"),
			     (system_present, "system-present",
			      "system-present value"));

SHELL_CMD_REGISTER(gpio, &sub_gpio_cmds,
		   "Checks the state of the GPIO pins on Dolos",
		   gpio_cmd_handler);

SHELL_CMD_REGISTER(status, NULL, "Prints Dolos status", status_cmd_handler);
