#pragma once

#include <filesystem>
#include <vector>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

std::vector<PriceBar> generate_prices(const Config& cfg, int seed_offset = 0);
void write_prices_csv(const std::filesystem::path& out_dir, const std::vector<PriceBar>& bars);

}  // namespace cpp_backtester
