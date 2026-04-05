#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

RollingOptimizationResult optimize_rolling_windows(const Config& cfg, const std::vector<PriceSeries>& input_series);
std::string rolling_optimization_result_to_json(const RollingOptimizationResult& result);
void write_rolling_optimization_artifacts(const std::filesystem::path& out_dir, const RollingOptimizationResult& result);

}  // namespace cpp_backtester
