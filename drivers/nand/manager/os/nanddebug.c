/**
 * nanddebug.c
 **/

#include <linux/kernel.h>

int nm_dbg_level = 3;

static int __init nddebug_setup(char *str)
{
	get_option(&str, &nm_dbg_level);
	return 1;
}

__setup("nddebug=", nddebug_setup);
