#include "cpp_backtester/monte_carlo.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

#include "cpp_backtester/price_generator.hpp"
#include "cpp_backtester/simulator.hpp"

namespace cpp_backtester {

McAggregate run_monte_carlo(const Config& cfg) {
    std::vector<double> terminal_equities;
    std::vector<double> drawdowns;
    terminal_equities.reserve(static_cast<std::size_t>(cfg.num_simulations));
    drawdowns.reserve(static_cast<std::size_t>(cfg.num_simulations));

    for (int i = 0; i < cfg.num_simulations; ++i) {
        const auto bars = generate_prices(cfg, i + 1);
        const auto artifacts = run_simulation(cfg, bars);
        terminal_equities.push_back(artifacts.summary.terminal_equity);
        drawdowns.push_back(artifacts.summary.max_drawdown);
    }

    const double mean_equity = std::accumulate(terminal_equities.begin(), terminal_equities.end(), 0.0) /
        std::max<std::size_t>(1, terminal_equities.size());
    const double mean_dd = std::accumulate(drawdowns.begin(), drawdowns.end(), 0.0) /
        std::max<std::size_t>(1, drawdowns.size());
    const auto [min_it, max_it] = std::minmax_element(terminal_equities.begin(), terminal_equities.end());
    const int positives = static_cast<int>(std::count_if(terminal_equities.begin(), terminal_equities.end(), [&](double x) {
        return x > cfg.initial_cash;
    }));

    return McAggregate{
        cfg.num_simulations,
        mean_equity,
        terminal_equities.empty() ? 0.0 : *min_it,
        terminal_equities.empty() ? 0.0 : *max_it,
        mean_dd,
        cfg.num_simulations > 0 ? static_cast<double>(positives) / static_cast<double>(cfg.num_simulations) : 0.0,
    };
}

std::string monte_carlo_to_json(const Config& cfg, const McAggregate& mc) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "{\n";
    os << "  \"num_simulations_requested\": " << cfg.num_simulations << ",\n";
    os << "  \"num_simulations_completed\": " << mc.simulations << ",\n";
    os << "  \"mean_terminal_equity\": " << mc.mean_terminal_equity << ",\n";
    os << "  \"min_terminal_equity\": " << mc.min_terminal_equity << ",\n";
    os << "  \"max_terminal_equity\": " << mc.max_terminal_equity << ",\n";
    os << "  \"mean_max_drawdown\": " << mc.mean_max_drawdown << ",\n";
    os << "  \"positive_run_ratio\": " << mc.positive_run_ratio << ",\n";
    os << "  \"used_jump_diffusion\": true,\n";
    os << "  \"used_atr_thresholds\": true\n";
    os << "}\n";
    return os.str();
}

}  // namespace cpp_backtester
