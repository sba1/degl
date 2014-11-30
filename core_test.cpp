#include "core.cpp"

#include <assert.h>

static void test__classify_args()
{
	const char *args[] = {"simplelib/func1.c", "simplelib/func2.c", "--", "-I."};
	vector<const char *> filenames;
	vector<const char *> options;

	classify_args(filenames, options, sizeof(args)/sizeof(args[0]), args);

	assert(filenames.size() == 2);
	assert(options.size() == 1);
}

int main(int argc, char **argv)
{
	test__classify_args();

	std::vector<const char *> files;
	files.push_back("simplelib/func1.c");
	files.push_back("simplelib/func2.c");

	std::vector<const char *> options;
	transform(files, options);
	return 0;
}
