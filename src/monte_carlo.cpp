#include "cpp_backtester/monte_carlo.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "cpp_backtester/price_generator.hpp"
#include "cpp_backtester/simulator.hpp"

namespace cpp_backtester {
namespace {

double compute_ratio(double numerator_pct, double max_drawdown_pct) {
    const double denominator = std::max(std::abs(max_drawdown_pct), 1e-6);
    return numerator_pct / denominator;
}

double compute_cagr_pct(double terminal_equity, double initial_cash, int num_days) {
    if (num_days <= 0 || initial_cash <= 0.0 || terminal_equity <= 0.0) {
        return 0.0;
    }
    const double years = static_cast<double>(num_days) / 252.0;
    if (years <= 0.0) {
        return 0.0;
    }
    return 100.0 * (std::pow(terminal_equity / initial_cash, 1.0 / years) - 1.0);
}

McAggregate aggregate_from_summaries(const Config& cfg, const std::vector<SimSummary>& summaries, const std::string& input_mode, int input_file_count) {
    std::vector<double> terminal_equities;
    std::vector<double> drawdowns;
    std::vector<double> total_returns;
    std::vector<double> cagrs;
    terminal_equities.reserve(summaries.size());
    drawdowns.reserve(summaries.size());
    total_returns.reserve(summaries.size());
    cagrs.reserve(summaries.size());

    for (const auto& summary : summaries) {
        terminal_equities.push_back(summary.terminal_equity);
        drawdowns.push_back(summary.max_drawdown);
        total_returns.push_back(100.0 * (summary.terminal_equity - cfg.initial_cash) / std::max(1.0, cfg.initial_cash));
        cagrs.push_back(compute_cagr_pct(summary.terminal_equity, cfg.initial_cash, summary.num_days));
    }

    const double mean_equity = std::accumulate(terminal_equities.begin(), terminal_equities.end(), 0.0) /
        std::max<std::size_t>(1, terminal_equities.size());
    const double mean_dd = std::accumulate(drawdowns.begin(), drawdowns.end(), 0.0) /
        std::max<std::size_t>(1, drawdowns.size());
    const double mean_total_return_pct = std::accumulate(total_returns.begin(), total_returns.end(), 0.0) /
        std::max<std::size_t>(1, total_returns.size());
    const double mean_cagr_pct = std::accumulate(cagrs.begin(), cagrs.end(), 0.0) /
        std::max<std::size_t>(1, cagrs.size());
    const auto [min_it, max_it] = std::minmax_element(terminal_equities.begin(), terminal_equities.end());
    const int positives = static_cast<int>(std::count_if(terminal_equities.begin(), terminal_equities.end(), [&](double x) {
        return x > cfg.initial_cash;
    }));

    const double mean_max_drawdown_pct = 100.0 * std::abs(mean_dd);
    const double cagr_over_drawdown_ratio = compute_ratio(mean_cagr_pct, mean_max_drawdown_pct);

    McAggregate aggregate{
        static_cast<int>(summaries.size()),
        mean_equity,
        terminal_equities.empty() ? 0.0 : *min_it,
        terminal_equities.empty() ? 0.0 : *max_it,
        mean_dd,
        summaries.empty() ? 0.0 : static_cast<double>(positives) / static_cast<double>(summaries.size()),
        mean_total_return_pct,
        mean_cagr_pct,
        mean_max_drawdown_pct,
        cagr_over_drawdown_ratio,
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
    os << "  \"mean_cagr_pct\": " << mc.mean_cagr_pct << ",\n";
    os << "  \"mean_max_drawdown_pct\": " << mc.mean_max_drawdown_pct << ",\n";
    os << "  \"cagr_over_drawdown_ratio\": " << mc.cagr_over_drawdown_ratio << ",\n";
    os << "  \"used_atr_thresholds\": true,\n";
    os << "  \"used_jump_diffusion\": " << (mc.input_mode == "synthetic_monte_carlo" ? "true" : "false") << "\n";
    os << "}\n";
    return os.str();
}

}  // namespace cpp_backtester
