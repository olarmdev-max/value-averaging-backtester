#pragma once

#include <filesystem>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

Config load_config(const std::filesystem::path& path);

}  // namespace cpp_backtester
