static int global;

void set_global(int value)
{
	int local = value;
	global = local;
}

int get_global(void)
{
	return global;
}

