#include "simplelib.h"

static int global;
static short sh_global = 2;

void set_global(int value)
{
	int local = value;
	global = local;
}

int get_global(void)
{
	return global;
}

void set_sh_global(short value)
{
	sh_global = value;
}

short get_sh_global(void)
{
	return sh_global;
}
