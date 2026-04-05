#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

SimulationArtifacts run_simulation(const Config& cfg, const std::vector<PriceBar>& bars);
void write_simulation_artifacts(const std::filesystem::path& out_dir, const SimulationArtifacts& artifacts);
std::string summary_to_json(const Config& cfg, const SimSummary& summary);

}  // namespace cpp_backtester
