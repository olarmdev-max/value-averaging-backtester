#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

OptimizationResult optimize_config(const Config& cfg, const std::vector<PriceSeries>& input_series = {});
std::string optimization_result_to_json(const OptimizationResult& result);
void write_optimization_artifacts(const std::filesystem::path& out_dir, const OptimizationResult& result);
bool dlib_backend_available();

}  // namespace cpp_backtester
