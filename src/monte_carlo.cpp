#include "cpp_backtester/monte_carlo.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "cpp_backtester/price_generator.hpp"
#include "cpp_backtester/simulator.hpp"

namespace cpp_backtester {
namespace {

double compute_return_over_drawdown_ratio(double total_return_pct, double max_drawdown_pct) {
    const double denominator = std::max(std::abs(max_drawdown_pct), 1e-6);
    return total_return_pct / denominator;
}

McAggregate aggregate_from_summaries(const Config& cfg, const std::vector<SimSummary>& summaries, const std::string& input_mode, int input_file_count) {
    std::vector<double> terminal_equities;
    std::vector<double> drawdowns;
    terminal_equities.reserve(summaries.size());
    drawdowns.reserve(summaries.size());

    for (const auto& summary : summaries) {
        terminal_equities.push_back(summary.terminal_equity);
        drawdowns.push_back(summary.max_drawdown);
    }

    const double mean_equity = std::accumulate(terminal_equities.begin(), terminal_equities.end(), 0.0) /
        std::max<std::size_t>(1, terminal_equities.size());
    const double mean_dd = std::accumulate(drawdowns.begin(), drawdowns.end(), 0.0) /
        std::max<std::size_t>(1, drawdowns.size());
    const auto [min_it, max_it] = std::minmax_element(terminal_equities.begin(), terminal_equities.end());
    const int positives = static_cast<int>(std::count_if(terminal_equities.begin(), terminal_equities.end(), [&](double x) {
        return x > cfg.initial_cash;
    }));

    const double mean_total_return_pct = 100.0 * (mean_equity - cfg.initial_cash) / std::max(1.0, cfg.initial_cash);
    const double mean_max_drawdown_pct = 100.0 * std::abs(mean_dd);
    const double return_over_drawdown_ratio = compute_return_over_drawdown_ratio(mean_total_return_pct, mean_max_drawdown_pct);

    McAggregate aggregate{
        static_cast<int>(summaries.size()),
        mean_equity,
        terminal_equities.empty() ? 0.0 : *min_it,
        terminal_equities.empty() ? 0.0 : *max_it,
        mean_dd,
        summaries.empty() ? 0.0 : static_cast<double>(positives) / static_cast<double>(summaries.size()),
        mean_total_return_pct,
        mean_max_drawdown_pct,
        return_over_drawdown_ratio,
        input_mode,
        input_file_count,
    };
    return aggregate;
}

}  // namespace

McAggregate run_monte_carlo(const Config& cfg) {
    std::vector<SimSummary> summaries;
    summaries.reserve(static_cast<std::size_t>(cfg.num_simulations));

    for (int i = 0; i < cfg.num_simulations; ++i) {
        const auto bars = generate_prices(cfg, i + 1);
        const auto artifacts = run_simulation(cfg, bars);
        summaries.push_back(artifacts.summary);
    }

    return aggregate_from_summaries(cfg, summaries, "synthetic_monte_carlo", 0);
}

McAggregate run_price_series_batch(const Config& cfg, const std::vector<PriceSeries>& series_list) {
    std::vector<SimSummary> summaries;
    summaries.reserve(series_list.size());
    for (const auto& series : series_list) {
        const auto artifacts = run_simulation(cfg, series.bars);
        summaries.push_back(artifacts.summary);
    }
    return aggregate_from_summaries(cfg, summaries, "input_files", static_cast<int>(series_list.size()));
}

std::string monte_carlo_to_json(const Config& cfg, const McAggregate& mc) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "{\n";
    os << "  \"num_simulations_requested\": " << cfg.num_simulations << ",\n";
    os << "  \"num_simulations_completed\": " << mc.simulations << ",\n";
    os << "  \"input_mode\": \"" << mc.input_mode << "\",\n";
    os << "  \"input_file_count\": " << mc.input_file_count << ",\n";
    os << "  \"mean_terminal_equity\": " << mc.mean_terminal_equity << ",\n";
    os << "  \"min_terminal_equity\": " << mc.min_terminal_equity << ",\n";
    os << "  \"max_terminal_equity\": " << mc.max_terminal_equity << ",\n";
    os << "  \"mean_max_drawdown\": " << mc.mean_max_drawdown << ",\n";
    os << "  \"positive_run_ratio\": " << mc.positive_run_ratio << ",\n";
    os << "  \"mean_total_return_pct\": " << mc.mean_total_return_pct << ",\n";
    os << "  \"mean_max_drawdown_pct\": " << mc.mean_max_drawdown_pct << ",\n";
    os << "  \"return_over_drawdown_ratio\": " << mc.return_over_drawdown_ratio << ",\n";
    os << "  \"used_atr_thresholds\": true,\n";
    os << "  \"used_jump_diffusion\": " << (mc.input_mode == "synthetic_monte_carlo" ? "true" : "false") << "\n";
    os << "}\n";
    return os.str();
}

}  // namespace cpp_backtester
