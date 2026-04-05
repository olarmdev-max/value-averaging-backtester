#pragma once

#include <filesystem>
#include <vector>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

std::vector<PriceBar> generate_prices(const Config& cfg, int seed_offset = 0);
std::vector<PriceBar> build_bars_from_closes(const std::vector<double>& closes, int atr_window);
void write_prices_csv(const std::filesystem::path& out_dir, const std::vector<PriceBar>& bars);

}  // namespace cpp_backtester
