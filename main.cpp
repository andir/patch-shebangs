#include <cstdlib>
#include <iostream>
#include <vector>

#include "patch-shebangs.hpp"


int main(int argc, char* argv[]) {
	std::vector<std::string> args;
	{
		const std::vector<const char*> c_args(argv, argv + argc);
		for (const auto s : c_args) {
			args.push_back(std::string(s));
		}
	}

	auto it = args.begin();
	auto end = args.end();
	it++;
	if (it == end) {
		std::cerr << "No arguments given" << std::endl;
		return 1;
	}
	std::cerr << "Reading search and replacement strings from args" << std::endl;
	auto mapping = parse_mapping(it, end);

	const char* PATH = secure_getenv("PATH");
	const std::string env_path = PATH == NULL ? "" : PATH;
	auto executables = find_all_executables(env_path);
	auto targets = parse_targets(it, end);
	patch_targets(targets, mapping, executables);

	return 0;
}
