#pragma once

#include <filesystem>
#include <vector>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

PriceSeries load_close_series_csv(const std::filesystem::path& path, int atr_window);
std::vector<PriceSeries> load_close_series_csvs(const std::vector<std::filesystem::path>& paths, int atr_window);

}  // namespace cpp_backtester
