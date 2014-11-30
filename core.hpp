/**
 * @file
 */
#ifndef CORE_HPP_
#define CORE_HPP_

#include <vector>

/**
 * Classifies the given arguments into filenames or compiler options.
 *
 * @param filenames output parameter that will contain all files to be considered
 * @param options output parameter that will contain all further options.
 * @param argc number of proper arguments (without the program name)
 * @param argv the actual proper arguments (without the program name)
 */
void classify_args(std::vector<const char *> &filenames, std::vector<const char *> &options, int argc, const char **argv);

void transform(std::vector<const char *> &filenames, std::vector<const char*> &options);

#endif /* CORE_HPP_ */
