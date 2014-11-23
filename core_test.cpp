#include "core.cpp"

int main(int argc, char **argv)
{
	std::vector<const char *> files;
	files.push_back("simplelib/func1.c");
	files.push_back("simplelib/func2.c");
	transform(files);
	return 0;
}
