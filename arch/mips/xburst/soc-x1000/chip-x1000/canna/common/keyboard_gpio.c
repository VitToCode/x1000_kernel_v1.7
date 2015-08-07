#include <linux/platform_device.h>

#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include "board_base.h"

#define KEY_R0 GPIO_PB(3)
#define KEY_R1 GPIO_PB(2)
#define KEY_R2 GPIO_PB(1)
#define KEY_R3 GPIO_PB(0)

#define KEY_C0 GPIO_PB(24)
#define KEY_C1 GPIO_PB(23)
#define KEY_C2 GPIO_PB(4)

static const unsigned int matrix_keypad_cols[] = {KEY_C0,KEY_C1,KEY_C2};
static const unsigned int matrix_keypad_rows[] = {KEY_R0,KEY_R1,KEY_R2,KEY_R3};
static uint32_t canna_keymap[] =
{
	//KEY(row, col, keycode)
	KEY(0,0, KEY_MODE),		/* AP/STA */
	KEY(0,1, KEY_F1),		/* AIRKISS */
	KEY(0,2, KEY_POWER),
	KEY(1,0, KEY_VOLUMEDOWN),
	KEY(1,1, KEY_VOLUMEUP),
	KEY(1,2, KEY_PLAYPAUSE),
	KEY(2,0, KEY_PREVIOUSSONG),
	KEY(2,1, KEY_NEXTSONG),
	KEY(2,2, KEY_MENU),
	KEY(3,0, KEY_RESERVED),
	KEY(3,1, KEY_BLUETOOTH),
	KEY(3,2, KEY_RECORD),
};

static struct matrix_keymap_data canna_keymap_data =
{
	.keymap = canna_keymap,
	.keymap_size =ARRAY_SIZE(canna_keymap),
};

struct matrix_keypad_platform_data canna_keypad_data  =
{
	.keymap_data = &canna_keymap_data,
	.col_gpios = matrix_keypad_cols,
	.row_gpios = matrix_keypad_rows,
	.num_col_gpios = ARRAY_SIZE(matrix_keypad_cols),
	.num_row_gpios = ARRAY_SIZE(matrix_keypad_rows),
	.col_scan_delay_us = 10,
	.debounce_ms =100,
	.wakeup  = 1,
	.active_low  = 1,
	.no_autorepeat = 1,
};

struct platform_device jz_matrix_kdb_device = {
	.name		= "matrix-keypad",
	.id		= 0,
	.dev		= {
		.platform_data  = &canna_keypad_data,
	}
};
