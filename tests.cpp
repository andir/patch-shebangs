#include <catch2/catch.hpp>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "patch-shebangs.hpp"

TEST_CASE("parse mapping from vector one entry", "args") {
	std::vector<std::string> args {
		"/bin/sh=/nix/store/bash",
	};
	auto it = args.begin();
	auto mapping = parse_mapping(it, args.end());
	auto res = mapping.find("/bin/sh");
	REQUIRE(res != mapping.end());
	REQUIRE(res->second == "/nix/store/bash");
}


TEST_CASE("parse mapping from vector multiple entries", "args") {
	std::vector<std::string> args {
		"/bin/sh=/nix/store/bash",
		"/bin/ash=/nix/store/ash",
		"/bin/dash=/nix/store/something",
	};
	auto it = args.begin();
	auto mapping = parse_mapping(it, args.end());

	REQUIRE(mapping == (map_t){
			{ "/bin/sh", "/nix/store/bash" },
			{ "/bin/ash", "/nix/store/ash" },
			{ "/bin/dash", "/nix/store/something" },
			});
}

TEST_CASE("parse mapping from PATH", "environment") {
	auto paths = split_env_path("/nix/store/a:/nix/store/b:/some/other/path");
	REQUIRE(
			paths == (std::vector<std::string>){
			"/nix/store/a",
			"/nix/store/b",
			"/some/other/path"
			}
	);
}

inline bool str_endswith(const std::string &value, const std::string &ending)
{
    if (ending.size() > value.size()) {
	    return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

TEST_CASE("find all executables must not barf on non-existant dir", "environment") {
	const auto path = "/no-such-directory";
	const auto res = find_executables(path);
	REQUIRE(res.size() == 0);
}

TEST_CASE("find all executables in directory", "environment") {
	const auto path = "@testPath@";
	const auto res = find_executables(path);
	REQUIRE(res.size() == 1);
	const auto e = res.find("executable");
	REQUIRE(e != res.end());
	REQUIRE(e->first == "executable");
	REQUIRE(str_endswith(e->second, "/executable"));
}

TEST_CASE("find all files given", "args") {
	const auto path = std::string("@testPath@");

	std::vector<std::string> args = {
		(path + "/executable"),
		(path + "/non-executable"),
		(path + "/subdir"),
		(path + "/nothing"),
	};

	auto it = args.begin();
	const auto res = parse_targets(it, args.end());
	REQUIRE(res == std::vector<std::filesystem::path> {
		(path + "/executable"),
		(path + "/non-executable"),
		(path + "/subdir"),
	});
	// REQUIRE(std::get<0>(res[0]) == "executable");
	// REQUIRE(str_endswith(std::get<1>(res[0]), "/executable"));
}


TEST_CASE("patch a single file's /bin/sh", "patch_file") {
	const auto text = "#! /bin/sh\ntest";
	const std::string tmpf = std::tmpnam(nullptr);
	{
		auto fh = std::ofstream(tmpf);
		fh << text;
		fh.flush();
		fh.close();
	}

	patch_file(tmpf, {{"/bin/sh", "/something/else"}}, {});

	{
		auto fh = std::ifstream(tmpf);
		std::string line;
		std::getline(fh, line);
		REQUIRE(line == "#! /something/else");
	}

	std::filesystem::remove(tmpf);
}

TEST_CASE("patch a single file's /bin/sh with arg", "patch_file") {
	const auto text = "#! /bin/sh -e\ntest";
	const std::string tmpf = std::tmpnam(nullptr);
	{
		auto fh = std::ofstream(tmpf);
		fh << text;
		fh.flush();
		fh.close();
	}

	patch_file(tmpf, {{"/bin/sh", "/something/else"}}, {});

	{
		std::cerr << "Reading patched file from " << tmpf << std::endl;
		auto fh = std::ifstream(tmpf);
		std::string line;
		std::getline(fh, line);
		REQUIRE(line == "#! /something/else -e");
		std::getline(fh, line);
		REQUIRE(line == "test");
	}

	std::filesystem::remove(tmpf);
}

TEST_CASE("patch a single file's /usr/bin/env sh with arg based on env variables", "patch_file") {
	const auto text = "#! /usr/bin/env sh -e\ntest";
	const std::string tmpf = std::tmpnam(nullptr);
	{
		auto fh = std::ofstream(tmpf);
		fh << text;
		fh.flush();
		fh.close();
	}

	patch_file(tmpf, {}, {{"sh", "/somewhere/sh"}});

	{
		std::cerr << "Reading patched file from " << tmpf << std::endl;
		auto fh = std::ifstream(tmpf);
		std::string line;
		std::getline(fh, line);
		REQUIRE(line == "#! /somewhere/sh -e");
		std::getline(fh, line);
		REQUIRE(line == "test");
	}

	std::filesystem::remove(tmpf);
}


TEST_CASE("patch a single file's /usr/bin/env sh with arg based on env variables without additional arguments", "patch_file") {
	const auto text = "#! /usr/bin/env sh\ntest";
	const std::string tmpf = std::tmpnam(nullptr);
	{
		auto fh = std::ofstream(tmpf);
		fh << text;
		fh.flush();
		fh.close();
	}

	patch_file(tmpf, {}, {{"sh", "/somewhere/sh"}});

	{
		std::cerr << "Reading patched file from " << tmpf << std::endl;
		auto fh = std::ifstream(tmpf);
		std::string line;
		std::getline(fh, line);
		REQUIRE(line == "#! /somewhere/sh");
		std::getline(fh, line);
		REQUIRE(line == "test");
	}

	std::filesystem::remove(tmpf);
}
