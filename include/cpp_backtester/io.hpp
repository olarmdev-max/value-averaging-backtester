#pragma once

#include <filesystem>
#include <string>

namespace cpp_backtester {

std::string read_file(const std::filesystem::path& path);
void write_text(const std::filesystem::path& path, const std::string& content);

}  // namespace cpp_backtester
