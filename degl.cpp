/**
 * @file
 */

#include "core.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--help"))
		{
			printf("Usage: %s [FILE]... -- [OPTION]...\n", argv[0]);
			printf("Transform all FILEs supplying for each the given OPTIONs to the compiler\n.");
			exit(0);
		}
	}

	vector<const char *> filenames;
	vector<const char *> options;
	classify_args(filenames, options, argc - 1, &argv[1]);
	transform(filenames, options);
	return 0;
}
