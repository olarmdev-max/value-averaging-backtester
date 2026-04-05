#pragma once

#include <string>
#include <vector>

#include "cpp_backtester/types.hpp"

namespace cpp_backtester {

McAggregate run_monte_carlo(const Config& cfg);
McAggregate run_price_series_batch(const Config& cfg, const std::vector<PriceSeries>& series_list);
std::string monte_carlo_to_json(const Config& cfg, const McAggregate& mc);

}  // namespace cpp_backtester
