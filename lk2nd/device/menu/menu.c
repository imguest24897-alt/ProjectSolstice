// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2023 Nikita Travkin <nikita@trvn.ru> */

#include <compiler.h>
#include <config.h>
#include <debug.h>
#include <dev/fbcon.h>
#include <display_menu.h>
#include <kernel/thread.h>
#include <platform/timer.h>
#include <platform.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <lk2nd/device/keys.h>
#include <lk2nd/util/minmax.h>
#include <lk2nd/version.h>

#include "../device.h"

// Defined in app/aboot/aboot.c
extern void cmd_continue(const char *arg, void *data, unsigned sz);

#define FONT_WIDTH	(5+1)
#define FONT_HEIGHT	12
#define MIN_LINE	40

enum fbcon_colors {
	WHITE = FBCON_TITLE_MSG,
	SILVER = FBCON_SUBTITLE_MSG,
	YELLOW = FBCON_YELLOW_MSG,
	ORANGE = FBCON_ORANGE_MSG,
	RED = FBCON_RED_MSG,
	GREEN = FBCON_GREEN_MSG,
	WHITE_ON_BLUE = FBCON_SELECT_MSG_BG_COLOR,
};

static int scale_factor = 0;

static void fbcon_puts(const char *str, unsigned type, int y, bool center)
{
	struct fbcon_config *fb = fbcon_display();
	int line_len = fb->width;
	int text_len = strlen(str) * FONT_WIDTH * scale_factor;
	int x = 0;

	if (center)
		x = (line_len - min(text_len, line_len)) / 2;

	while (*str != 0) {
		fbcon_putc_factor_xy(*str++, type, scale_factor, x, y);
		x += FONT_WIDTH * scale_factor;
		if (x >= line_len)
			return;
	}
}

static __PRINTFLIKE(4, 5) void fbcon_printf(unsigned type, int y, bool center, const char *fmt, ...)
{
	char buf[MIN_LINE * 3];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fbcon_puts(buf, type, y, center);
}

static const uint16_t published_keys[] = {
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_POWER,
	KEY_HOME,
};

/**
 * lk2nd_boot_pressed_key() - Get the pressed key, if any.
 */
static uint16_t lk2nd_boot_pressed_key(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(published_keys); ++i)
		if (lk2nd_keys_pressed(published_keys[i]))
			return published_keys[i];

	return 0;
}

#define LONG_PRESS_DURATION 1000

static uint16_t wait_key(void)
{
	uint16_t keycode = 0;
	int press_start = 0;
	int press_duration;

	while (!(keycode = lk2nd_boot_pressed_key()))
		thread_sleep(1);

	press_start = current_time();

	while (lk2nd_keys_pressed(keycode)) {
		thread_sleep(1);

		press_duration = current_time() - press_start;
		if (lk2nd_dev.single_key && press_duration > LONG_PRESS_DURATION)
			return KEY_POWER;
	}

	if (lk2nd_dev.single_key)
		keycode = KEY_VOLUMEDOWN;

	/* A small debounce delay */
	thread_sleep(5);

	return keycode;
}

#define xstr(s) str(s)
#define str(s) #s

#define fbcon_printf_ln(color, y, incr, x...) \
	do { \
		fbcon_printf(color, y, x); \
		y += incr; \
	} while(0)

#define fbcon_puts_ln(color, y, incr, center, str) \
	do { \
		fbcon_puts(str, color, y, center); \
		y += incr; \
	} while(0)

static void opt_continue(void)   {
	struct fbcon_config *fb = fbcon_display();
	int y, incr;
	scale_factor = max(1U, min(fb->width, fb->height) / (FONT_WIDTH * MIN_LINE));
	incr = FONT_HEIGHT * scale_factor;
	y = fb->height - 3 * incr;

	fbcon_clear_msg(y / FONT_HEIGHT, y / FONT_HEIGHT + 3 * scale_factor);

	fbcon_clear();

	const char *s = "Project Solstice";
	int scale = 7;
	for (int i = 0; s[i]; ++i)
    	fbcon_putc_factor_xy(s[i], FBCON_COMMON_MSG, scale,
        	(fbcon_get_width() - (int)strlen(s) * 6 * scale) / 2 + i * 6 * scale,
        	(fbcon_get_height() - 12 * scale) / 2);
	fbcon_flush();
	const char *s2;
	static const char *random_boot_messages[] = {
		"the bluetooth device is ready to pair",
		"alpha :P",
		"this is totally not lk trust me",
		"time to enter fastboot all over again",
		"undefined",
		"oneshot",
		"totally not a dirty flash i swear",
		"i hope this boots",
		"i hope this doesnt boot",
		"powered by android",
		"waiting for something to explode",
		"...",
		"i have nothing to say",
		"asdasdasdasdasdasdasdasd",
		"always on display more like fire on display",
		"todo: probably make this more interesting",
		"this totally wont explode",
		"running on a toaster",
		"mediatek jumpscare",
		"A N D R O I D",
		"welcome to lk",
		"idk",
		"android in the bios",
		"\"JARONA\"  - flowery",
		"proceed",
		"geometry dash",
		"a totally awesome second stage bootloader",
	};
	s2 = random_boot_messages[(int)current_time() % (sizeof(random_boot_messages) / 27)];
	int scale2 = 3;
	for (int i = 0; s2[i]; ++i)
    	fbcon_putc_factor_xy(s2[i], FBCON_COMMON_MSG, scale2,
        	(fbcon_get_width() - (int)strlen(s2) * 6 * scale2) / 2 + i * 6 * scale2,
        	(fbcon_get_height() - -22 * scale2) / 2);
	fbcon_flush();

	fbcon_puts_ln(WHITE, y, incr, true, "Powered by");
	fbcon_puts_ln(SILVER, y, incr, true, "lk2nd");
	fbcon_flush();
	cmd_continue(NULL, NULL, 0);
}
static void opt_reboot(void)     { reboot_device(0); }
static void opt_recovery(void)
{
	extern unsigned boot_into_recovery;

	boot_into_recovery = 1;
	cmd_continue(NULL, NULL, 0);
}
static void opt_bootloader(void) { reboot_device(FASTBOOT_MODE); }
static void opt_edl(void)        { reboot_device(EMERGENCY_DLOAD); }
static void opt_shutdown(void)   { shutdown_device(); }
static void opt_download(void)   { for (int i=0; "Rebooting into download mode isn't implemented yet!"[i]; ++i) fbcon_putc_factor_xy("Rebooting into download mode isn't implemented yet!"[i], FBCON_RED_MSG, 2, i * 6 * 2, 0); }

static struct {
	char *name;
	unsigned color;
	void (*action)(void);
} menu_options[] = {
	{ "REBOOT"  , GREEN , opt_reboot     },
	{ "CONTINUE", GREEN , opt_continue   },
	{ "RECOVERY", ORANGE, opt_recovery   },
	{ "DOWNLOAD", ORANGE, opt_download   },
	{ "FASTBOOT", RED   , opt_bootloader },
	{ "EDL"     , RED   , opt_edl        },
	{ "POWEROFF", RED   , opt_shutdown   },
};

void display_fastboot_menu(void)
{
	struct fbcon_config *fb = fbcon_display();
	int y, y_menu, old_scale, incr;
	unsigned int sel = 0, i;
	bool armv8 = is_scm_armv8_support();

	if (!fb)
		return;

	/*
	 * Make sure the specified line lenght fits on the screen.
	 */
	scale_factor = max(1U, min(fb->width, fb->height) / (FONT_WIDTH * MIN_LINE));
	old_scale = scale_factor;
	incr = FONT_HEIGHT * scale_factor;

	y = incr;

	fbcon_clear();

	/*
	 * Draw the static part of the menu
	 */

	scale_factor += 1;
	incr = FONT_HEIGHT * scale_factor;
	fbcon_puts_ln(WHITE, y, incr, false, " Project Solstice (ALPHA!)");
	scale_factor += 1; // idk what am i doing
	scale_factor -= 2;
	y += 10;
	fbcon_puts_ln(RED   , y, incr, false, "  FASTBOOT MODE");
	fbcon_printf_ln(SILVER, y, incr, false, "  %s", LK2ND_VERSION);
	if (lk2nd_dev.model)
		fbcon_printf_ln(SILVER, y, incr, false, "  %s", lk2nd_dev.model);
	else
		fbcon_printf_ln(SILVER, y, incr, false, "  unknown device (FIXME!)");
	scale_factor += 2;
	y -= (incr - 24);
	fbcon_puts_ln(WHITE, y, incr, false, "_______________________________________________");
	fbcon_puts_ln(WHITE, y, incr, false, "");

	scale_factor = old_scale;
	incr = FONT_HEIGHT * scale_factor;

	/* Skip lines for the menu */
	y_menu = y;
	y += incr * ((ARRAY_SIZE(menu_options) / 3) + 1);

	if (lk2nd_dev.single_key) {
		fbcon_puts_ln(SILVER, y, incr, false, " Short press to navigate.");
		fbcon_puts_ln(SILVER, y, incr, false, " Long press to select.");
	} else {
		fbcon_printf_ln(SILVER, y, incr, false, " You can use the %s to ",
				(lk2nd_dev.menu_keys.navigate ? lk2nd_dev.menu_keys.navigate : "volume buttons"));
		fbcon_printf_ln(SILVER, y, incr, false, " navigate, or %s to",
				(lk2nd_dev.menu_keys.select ? lk2nd_dev.menu_keys.select : "the power/home key"));
		fbcon_printf_ln(SILVER, y, incr, false, " select.");
	}

	/*
	 * Draw the device-specific information at the bottom of the screen
	 */

	scale_factor = max(1, scale_factor - 1);
	incr = FONT_HEIGHT * scale_factor;
	y = fb->height - 8 * incr;

	fbcon_puts_ln(SILVER, y, incr, true, "About this device");


	if (lk2nd_dev.panel.name)
		fbcon_printf_ln(WHITE, y, incr, false, " Panel:            %s", lk2nd_dev.panel.name);
	else
		fbcon_printf_ln(YELLOW, y, incr, false, " Panel:            ???");
	if (lk2nd_dev.battery) {
		fbcon_printf_ln(WHITE, y, incr, false, " Battery:          %s", lk2nd_dev.battery);
	} else {
		fbcon_printf_ln(YELLOW, y, incr, false, " Battery:          ???");
	}
#if WITH_LK2ND_DEVICE_2ND
	if (lk2nd_dev.bootloader)
		fbcon_printf_ln(WHITE, y, incr, false, " Bootloader:       %s", lk2nd_dev.bootloader);
	else
		fbcon_printf_ln(YELLOW, y, incr, false, " Bootloader:       ???");
#endif

	fbcon_printf_ln(armv8 ? GREEN : RED, y, incr, false, " ARM64 supported:  %s",
			armv8 ? "yes" : "no");

	/*
	 * Loop to render the menu elements
	 */

	scale_factor = 4;
	incr = FONT_HEIGHT * scale_factor;
	while (true) {
		y = y_menu;
		fbcon_clear_msg(y / FONT_HEIGHT, (y / FONT_HEIGHT + ARRAY_SIZE(menu_options) * (scale_factor / 3)));
		fbcon_printf_ln(
			i == sel ? menu_options[sel].color : WHITE,
			y, incr, false, " %c %s",
			i == sel ? '>' : ' ',
			menu_options[i].name
		);
		fbcon_puts_ln(WHITE, y, incr, false, "_______________________________________________");
		fbcon_puts_ln(WHITE, y, incr, false, "");

		fbcon_flush();

		switch (wait_key()) {
		case KEY_POWER:
		case KEY_HOME:
			menu_options[sel].action();
			break;
		case KEY_VOLUMEUP:
			if (sel != 0) {
				i--;
				sel--;
			} else {
				i = 6;
				sel = 6;
			}
			break;
		case KEY_VOLUMEDOWN:
			if (sel != 6) {
				i++;
				sel++;
			} else {
				i = 0;
				sel = 0;
			}
			break;
		}
	}
}

static unsigned int rand(unsigned int *state)
{
	unsigned int x = *state;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;

	*state = x;
	return x;
}

void display_default_image_on_screen(void);
void display_default_image_on_screen(void)
{
	struct fbcon_config *fb = fbcon_display();
	int y, incr;

	/*
	 * Make sure the specified line lenght fits on the screen.
	 */
	scale_factor = max(1U, min(fb->width, fb->height) / (FONT_WIDTH * MIN_LINE));
	incr = FONT_HEIGHT * scale_factor;
	y = fb->height - 3 * incr;

	fbcon_clear_msg(y / FONT_HEIGHT, y / FONT_HEIGHT + 3 * scale_factor);

	fbcon_clear();

	const char *s = "Project Solstice";
	int scale = 6;
	for (int i = 0; s[i]; ++i)
    	fbcon_putc_factor_xy(s[i], FBCON_COMMON_MSG, scale,
        	(fbcon_get_width() - (int)strlen(s) * 6 * scale) / 2 + i * 6 * scale,
        	(fbcon_get_height() - 12 * scale) / 2);
	const char *s2;
	static const char *random_boot_messages[] = {
		"the bluetooth device is ready to pair",
		"alpha :P",
		"this is totally not lk trust me",
		"time to enter fastboot all over again",
		"undefined",
		"oneshot",
		"totally not a dirty flash i swear",
		"i hope this boots",
		"i hope this doesnt boot",
		"powered by android",
		"waiting for something to explode",
		"...",
		"i have nothing to say",
		"asdasdasdasdasdasdasdasd",
		"always on display more like fire on display",
		"todo: probably make this more interesting",
		"this totally wont explode",
		"running on a toaster",
		"mediatek jumpscare",
		"A N D R O I D",
		"welcome to lk",
		"idk",
		"android in the bios",
		"\"JARONA\"  - flowery",
		"proceed",
		"geometry dash",
		"a totally awesome second stage bootloader",
	};
	s2 = random_boot_messages[(int)current_time() % (sizeof(random_boot_messages) / 27)];
	int scale2 = 3;
	for (int i = 0; s2[i]; ++i)
    	fbcon_putc_factor_xy(s2[i], FBCON_COMMON_MSG, scale2,
        	(fbcon_get_width() - (int)strlen(s2) * 6 * scale2) / 2 + i * 6 * scale2,
        	(fbcon_get_height() - -22 * scale2) / 2);
	fbcon_flush();

	fbcon_puts_ln(SILVER, y, incr, true, "Powered by");
	fbcon_puts_ln(WHITE, y, incr, true, "lk2nd");
	fbcon_flush();
}
