#include "cpp_backtester/price_generator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>

#include "cpp_backtester/io.hpp"

namespace cpp_backtester {

std::vector<PriceBar> generate_prices(const Config& cfg, int seed_offset) {
    std::mt19937_64 rng(static_cast<std::uint64_t>(cfg.seed + seed_offset * 7919));
    std::normal_distribution<double> normal(0.0, 1.0);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    const double dt = 1.0 / 252.0;
    std::vector<PriceBar> bars;
    std::vector<double> true_ranges;
    bars.reserve(static_cast<std::size_t>(cfg.num_days));
    true_ranges.reserve(static_cast<std::size_t>(cfg.num_days));

    double prev_close = cfg.start_price;
    for (int i = 0; i < cfg.num_days; ++i) {
        const double open = prev_close;
        const double diffusion = (cfg.mu - 0.5 * cfg.sigma * cfg.sigma) * dt + cfg.sigma * std::sqrt(dt) * normal(rng);
        const bool jump = uniform(rng) < cfg.jump_probability;
        const double jump_term = jump ? (cfg.jump_mean + cfg.jump_std * normal(rng)) : 0.0;
        double close = open * std::exp(diffusion + jump_term);
        close = std::max(close, 0.01);

        const double spread = 0.005 + 0.01 * std::abs(normal(rng)) + 0.15 * std::abs(jump_term);
        const double high = std::max(open, close) * (1.0 + spread);
        const double low = std::max(0.01, std::min(open, close) * (1.0 - spread));

        const double tr = std::max({high - low, std::abs(high - prev_close), std::abs(low - prev_close)});
        true_ranges.push_back(tr);

        const int start = std::max(0, static_cast<int>(true_ranges.size()) - cfg.atr_window);
        double atr = 0.0;
        for (int j = start; j < static_cast<int>(true_ranges.size()); ++j) {
            atr += true_ranges[static_cast<std::size_t>(j)];
        }
        atr /= static_cast<double>(true_ranges.size() - static_cast<std::size_t>(start));

        bars.push_back(PriceBar{i + 1, open, high, low, close, atr, jump});
        prev_close = close;
    }
    return bars;
}

void write_prices_csv(const std::filesystem::path& out_dir, const std::vector<PriceBar>& bars) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "day,open,high,low,close,atr,jump_applied\n";
    for (const auto& bar : bars) {
        os << bar.day << ',' << bar.open << ',' << bar.high << ',' << bar.low << ',' << bar.close << ',' << bar.atr << ',' << (bar.jump_applied ? 1 : 0) << "\n";
    }
    write_text(out_dir / "prices.csv", os.str());
}

}  // namespace cpp_backtester
