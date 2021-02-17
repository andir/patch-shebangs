#include "patch-shebangs.hpp"
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string_view>
#include <filesystem>
#include <cassert>

namespace fs = std::filesystem;
#if 1
#define DEBUG(x) { std::cerr << x << std::endl; }
#else
#define DEBUG(x) {}
#endif

static bool is_executable(const fs::path& path) {
	if (!fs::is_regular_file(path)) {
		return false;
	}

	if (access(path.c_str(), X_OK)) {
		return false;
	}
	return true;
}


static void write_and_rename(const std::string &filename,
		std::ifstream& file,
		const std::string &executable,
		const std::string_view args) {
	// retrieve the old file attributes so we can restore them
	const auto status = fs::status(filename);
	const auto permissions = status.permissions();

	const std::string tempname = std::tmpnam(nullptr);
	DEBUG("Writing new file to " << tempname);
	auto outf = std::ofstream(tempname);
	outf << "#! " << executable;
	if (args.length() > 0)
		outf << " " << args;
	outf << std::endl;

	std::vector<char> buffer(4096, 0);
	while (file.good()) {
               file.read(buffer.data(), buffer.size());
               const auto n = file.gcount();
               outf.write(buffer.data(), n);
               buffer.clear();
	}

	file.close();
	outf.flush();
	outf.close();
	DEBUG("renaming file " << tempname << " to " << filename);
	try {
		fs::rename(tempname, filename);
	} catch (fs::filesystem_error& e) {
		// likely a cross device rename, we have to copy instead
		fs::copy(tempname, filename, fs::copy_options::overwrite_existing);
	}
	fs::permissions(filename, permissions);
}


void patch_file(const std::string filename, const map_t &replacements, const map_t &executables) {

	std::ifstream file(filename);
	std::string line;
	// read the first line and replace references to the patterns, if it looks like a shebang
	std::getline(file, line);
	if (line.size() > 1024) {
		std::cerr << "skipping overly ling shebang line" << std::endl;
		return;
	}

	if (line.size() < 3) {
		DEBUG("line is too short");
	}

	DEBUG("read line: " << line);

	if (line.rfind("#!", 0) != 0) {
		DEBUG("line is missing #! prefix");
		return;
	}

	std::string_view s(line);
	s.remove_prefix(2); // remove "#!"

	while (s.length() > 0  && s[0] == ' ') {
		s.remove_prefix(1);
	}

	if (s.length() == 0) {
		DEBUG("only whitespaces, ignoring");
		return;
	}

	if (s[0] != '/') {
		std::cerr << "shebang doesn't start with a slash, ignoring" << std::endl;
		return;
	}

	// Find the first space (or EOL) after the actual command
	const size_t space_pos = s.find(" ");
	const auto binary_view = space_pos == std::string_view::npos ? s : s.substr(0, space_pos);
	DEBUG("binary = " << binary_view);
	DEBUG("space_pos = " << space_pos);
	const auto arg_view = space_pos == std::string_view::npos ? "" : s.substr(space_pos + 1 /* take the character after the space */, s.length());
	DEBUG("args = " << arg_view);
	const std::string binary = std::string{binary_view};

	if (binary == "/usr/bin/env") {
		const auto space_pos = arg_view.find(" ");
		DEBUG("env space_pos = " << space_pos);
		const auto executable = std::string{arg_view.substr(0, space_pos)};
		DEBUG("Looking for executable for " <<  executable);
		const auto args = space_pos != std::string_view::npos ? arg_view.substr(space_pos + 1, arg_view.length()) : "";
		DEBUG("Executable args = " << args);

		if (auto entry = executables.find(executable); entry != executables.end()) {
			write_and_rename(filename, file, entry->second, args);
		} else {
			std::cerr << "No executable found for " << executable << std::endl;
		}

	} else if (auto entry = replacements.find(binary); entry != replacements.end()) {
		DEBUG("Found mapping for " << binary << "=" << entry->second);
		const std::string tempname = std::tmpnam(nullptr);
		write_and_rename(filename, file, entry->second, arg_view);
	} else {
		DEBUG("No replacement for " << binary << " found");
	}
}


map_t parse_mapping(std::vector<std::string>::iterator &it, std::vector<std::string>::iterator end) { map_t mapping {};
	for (; it != end && *it != "--"; it++) {
		const auto s = *it;
		if (const auto f = s.find("="); f != std::string::npos) {
			const auto search = s.substr(0, f);
			const auto value = s.substr(f + 1);

			if (value.length() != 0) {
				mapping.emplace(search, value);
				std::cerr << "Will replace " << search << " with " << value << std::endl;
			}
		} else {
			break;
		}
	}
	return mapping;
}


std::vector<std::string> split_env_path(const std::string &PATH) {
	std::vector<std::string> paths {};

	std::stringstream ss(PATH);
	std::string path;
	while(std::getline(ss, path, ':')) {
		paths.push_back(path);
	}

	return paths;
}

map_t find_executables(const std::string &path) {
	map_t executables;
	const auto p = fs::path(path);
	if (!fs::exists(p)) {
		return {};
	}
	for (auto p : fs::directory_iterator(p)) {
		auto path = p.path();
		// We only want files or symlinks. Those symlinks should then
		// point to files but we do not enforce that yet.
		if (!fs::is_regular_file(path)) {
			continue;
		}

		if (access(path.c_str(), X_OK)) {
			continue;
		}

		executables.emplace(path.filename() , path);
	}

	return executables;
}

map_t find_all_executables(const std::string &PATH) {
	map_t executables;
	for (const auto path : split_env_path(PATH)) {
		map_t exs = find_executables(path);
		for (const auto& e : exs) {
			executables.try_emplace(e.first, e.second);
		}
	}

	return executables;
}

std::vector<std::filesystem::path> parse_targets(std::vector<std::string>::iterator &it, std::vector<std::string>::iterator end) {
	std::vector<std::filesystem::path> paths;

	for (; it != end && *it != "--"; it++) {
		const auto s = *it;
		const auto p = std::filesystem::path(s);
		if (std::filesystem::exists(p)) {
			paths.push_back(p);
		}
	}

	return paths;
}

void patch_targets(std::vector<fs::path> &targets, map_t &mapping, map_t &binaries) {
	for (auto& t : targets) {
		if (fs::is_directory(t)) {
			for (auto& p : fs::recursive_directory_iterator(t, fs::directory_options::skip_permission_denied)) {
				assert(fs::is_regular_file(p));
				if (is_executable(p))
					patch_file(p.path(), mapping, binaries);
			}
		} else {
			if (is_executable(t))
				patch_file(t, mapping, binaries);
		}
	}
}

