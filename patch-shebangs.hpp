#include <string>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <filesystem>

using map_t = std::unordered_map<std::string, std::string>;

void patch_file(const std::string filename, const map_t &replacements, const map_t &binaries);
map_t parse_mapping(std::vector<std::string>::iterator &it, std::vector<std::string>::iterator end);
std::vector<std::filesystem::path> parse_targets(std::vector<std::string>::iterator &it, std::vector<std::string>::iterator end);
void patch_targets(std::vector<std::filesystem::path> &targets, map_t &mapping, map_t &executables);

std::vector<std::string> split_env_path(const std::string &PATH);
map_t find_executables(const std::string &path);
map_t find_all_executables(const std::string &PATH);
