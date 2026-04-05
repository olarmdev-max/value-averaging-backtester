#include "cpp_backtester/simulator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "cpp_backtester/io.hpp"

namespace cpp_backtester {
namespace {

double aggressiveness_fraction(const Config& cfg) {
    switch (cfg.aggressiveness) {
        case 1: return cfg.agg_fraction_1;
        case 2: return cfg.agg_fraction_2;
        case 3: return cfg.agg_fraction_3;
        case 4: return cfg.agg_fraction_4;
        default: return cfg.agg_fraction_2;
    }
}

}  // namespace

SimulationArtifacts run_simulation(const Config& cfg, const std::vector<PriceBar>& bars) {
    double cash = cfg.initial_cash;
    double shares = 0.0;
    double avg_cost = 0.0;
    double peak_equity = cfg.initial_cash;
    double max_drawdown = 0.0;

    // Target-path state
    double target_invested = cfg.initial_cash;
    const double agg_frac = aggressiveness_fraction(cfg);

    std::vector<TradeRow> trades;
    std::vector<EquityRow> equity_rows;

    int buy_count = 0;
    int sell_count = 0;
    int reset_sell_count = 0;
    int overage_sell_count = 0;

    for (const auto& bar : bars) {
        const double close = bar.close;
        const double atr = std::max(bar.atr, 1e-6);
        bool action_taken = false;

        const double actual_invested = shares * close;

        // 1. Long-term reset check
        if (shares > 1e-9 && target_invested > 1e-9) {
            const bool profitable_enough = cfg.allow_sell_at_loss || close >= avg_cost;
            if (profitable_enough && actual_invested >= cfg.long_term_reset_threshold * target_invested) {
                const double qty = shares;
                const double proceeds = qty * close;
                const double gross_profit = (close - avg_cost) * qty;
                cash += proceeds;
                shares = 0.0;
                avg_cost = 0.0;
                trades.push_back(TradeRow{bar.day, "SELL_ALL_RESET", qty, close, cash, shares, gross_profit});
                ++sell_count;
                ++reset_sell_count;
                action_taken = true;

                // Reset path from new cash base
                target_invested = cash;
            }
        }

        // 2. Daily overage check
        if (!action_taken && shares > 1e-9 && target_invested > 1e-9) {
            const double actual_now = shares * close;
            if (actual_now >= cfg.daily_overage_threshold * target_invested) {
                const bool profitable_enough = cfg.allow_sell_at_loss || close >= avg_cost;
                if (profitable_enough) {
                    const double overage = actual_now - target_invested;
                    const double qty = std::min(overage / close, shares);
                    if (qty > 1e-9) {
                        const double proceeds = qty * close;
                        const double gross_profit = (close - avg_cost) * qty;
                        cash += proceeds;
                        shares -= qty;
                        if (shares <= 1e-9) {
                            shares = 0.0;
                            avg_cost = 0.0;
                        }
                        trades.push_back(TradeRow{bar.day, "SELL_OVERAGE", qty, close, cash, shares, gross_profit});
                        ++sell_count;
                        ++overage_sell_count;
                        action_taken = true;
                    }
                }
            }
        }

        // 3. Shortfall catch-up buy
        if (!action_taken && cash > 1e-6) {
            const double invested_now = shares * close;
            const double total = cash + invested_now;
            const double utilization = total > 0.0 ? invested_now / total : 0.0;

            if (utilization < cfg.cash_utilization_limit && target_invested > invested_now) {
                const double shortfall = target_invested - invested_now;
                const double desired_buy = shortfall * agg_frac;
                const double buy_amount = std::min(desired_buy, cash);
                if (buy_amount > 1.0) {
                    const double qty = buy_amount / close;
                    const double new_cost_basis = avg_cost * shares + buy_amount;
                    shares += qty;
                    avg_cost = new_cost_basis / shares;
                    cash -= buy_amount;
                    trades.push_back(TradeRow{bar.day, "BUY", qty, close, cash, shares, 0.0});
                    ++buy_count;
                }
            }
        }

        // Grow target path for next day
        const double atr_pct = atr / std::max(close, 1e-9);
        const double daily_rate = cfg.base_rate * (1.0 + cfg.k_atr_sensitivity * atr_pct);
        target_invested *= (1.0 + daily_rate);

        // Track equity and drawdown
        const double equity = cash + shares * close;
        peak_equity = std::max(peak_equity, equity);
        max_drawdown = std::min(max_drawdown, (equity - peak_equity) / std::max(peak_equity, 1e-9));
        equity_rows.push_back(EquityRow{bar.day, close, cash, 0.0, 0.0, shares, equity});
    }

    const double last_close = bars.empty() ? 0.0 : bars.back().close;
    const double terminal_equity = cash + shares * last_close;
    const double investable_total = cash + shares * last_close;
    const double utilization_final = investable_total > 0.0 ? (shares * last_close) / investable_total : 0.0;

    SimSummary summary{
        static_cast<int>(bars.size()),
        buy_count,
        sell_count,
        reset_sell_count,
        overage_sell_count,
        terminal_equity,
        cash,
        shares,
        0.0,
        0.0,
        max_drawdown,
        utilization_final,
    };

    return SimulationArtifacts{std::move(trades), std::move(equity_rows), summary};
}

void write_simulation_artifacts(const std::filesystem::path& out_dir, const SimulationArtifacts& artifacts) {
    std::ostringstream trades_csv;
    trades_csv << std::fixed << std::setprecision(6);
    trades_csv << "day,action,qty,price,cash_after,shares_after,gross_profit\n";
    for (const auto& row : artifacts.trades) {
        trades_csv << row.day << ',' << row.action << ',' << row.qty << ',' << row.price << ',' << row.cash_after << ',' << row.shares_after << ',' << row.gross_profit << "\n";
    }
    write_text(out_dir / "trade_log.csv", trades_csv.str());

    std::ostringstream equity_csv;
    equity_csv << std::fixed << std::setprecision(6);
    equity_csv << "day,close,cash,taxes,retained,shares,equity\n";
    for (const auto& row : artifacts.equity_rows) {
        equity_csv << row.day << ',' << row.close << ',' << row.cash << ',' << row.taxes << ',' << row.retained << ',' << row.shares << ',' << row.equity << "\n";
    }
    write_text(out_dir / "equity.csv", equity_csv.str());
}

std::string summary_to_json(const Config& cfg, const SimSummary& summary) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "{\n";
    os << "  \"num_days\": " << summary.num_days << ",\n";
    os << "  \"buy_count\": " << summary.buy_count << ",\n";
    os << "  \"sell_count\": " << summary.sell_count << ",\n";
    os << "  \"reset_sell_count\": " << summary.reset_sell_count << ",\n";
    os << "  \"overage_sell_count\": " << summary.overage_sell_count << ",\n";
    os << "  \"terminal_equity\": " << summary.terminal_equity << ",\n";
    os << "  \"terminal_cash\": " << summary.terminal_cash << ",\n";
    os << "  \"terminal_shares\": " << summary.terminal_shares << ",\n";
    os << "  \"taxes_retained\": " << summary.taxes_retained << ",\n";
    os << "  \"profit_retained\": " << summary.profit_retained << ",\n";
    os << "  \"max_drawdown\": " << summary.max_drawdown << ",\n";
    os << "  \"utilization_final\": " << summary.utilization_final << ",\n";
    os << "  \"strategy_model\": \"target_path\",\n";
    os << "  \"base_rate\": " << cfg.base_rate << ",\n";
    os << "  \"k_atr_sensitivity\": " << cfg.k_atr_sensitivity << ",\n";
    os << "  \"long_term_reset_threshold\": " << cfg.long_term_reset_threshold << ",\n";
    os << "  \"daily_overage_threshold\": " << cfg.daily_overage_threshold << ",\n";
    os << "  \"aggressiveness\": " << cfg.aggressiveness << ",\n";
    os << "  \"allow_sell_at_loss\": " << (cfg.allow_sell_at_loss ? "true" : "false") << "\n";
    os << "}\n";
    return os.str();
}

}  // namespace cpp_backtester
