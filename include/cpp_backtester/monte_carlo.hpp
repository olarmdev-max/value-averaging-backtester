#pragma once

#include <string>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

McAggregate run_monte_carlo(const Config& cfg);
std::string monte_carlo_to_json(const Config& cfg, const McAggregate& mc);

}  // namespace cpp_backtester
