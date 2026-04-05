#include "cpp_backtester/simulator.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "cpp_backtester/io.hpp"

namespace cpp_backtester {

SimulationArtifacts run_simulation(const Config& cfg, const std::vector<PriceBar>& bars) {
    double cash = cfg.initial_cash;
    double taxes = 0.0;
    double retained = 0.0;
    double shares = 0.0;
    double avg_cost = 0.0;
    double peak_equity = cfg.initial_cash;
    double max_drawdown = 0.0;

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

        if (shares > 1e-9) {
            const double edge_atr = (close - avg_cost) / atr;
            const bool profitable_enough = cfg.allow_sell_at_loss || close >= avg_cost;

            if (profitable_enough && edge_atr >= cfg.long_term_target_atr) {
                const double qty = shares;
                const double proceeds = qty * close;
                const double gross_profit = (close - avg_cost) * qty;
                const double tax_cut = gross_profit > 0.0 ? gross_profit * cfg.taxes_retained_pct : 0.0;
                const double retained_cut = gross_profit > 0.0 ? gross_profit * cfg.profit_retained_pct : 0.0;
                cash += proceeds - tax_cut - retained_cut;
                taxes += tax_cut;
                retained += retained_cut;
                shares = 0.0;
                avg_cost = 0.0;
                trades.push_back(TradeRow{bar.day, "SELL_ALL_RESET", qty, close, cash, shares, gross_profit});
                ++sell_count;
                ++reset_sell_count;
                action_taken = true;
            } else if (profitable_enough && edge_atr >= cfg.daily_target_atr) {
                const double fraction = std::clamp(0.10 + 0.05 * (edge_atr - cfg.daily_target_atr), 0.10, 0.35);
                const double qty = shares * fraction;
                const double proceeds = qty * close;
                const double gross_profit = (close - avg_cost) * qty;
                const double tax_cut = gross_profit > 0.0 ? gross_profit * cfg.taxes_retained_pct : 0.0;
                const double retained_cut = gross_profit > 0.0 ? gross_profit * cfg.profit_retained_pct : 0.0;
                cash += proceeds - tax_cut - retained_cut;
                taxes += tax_cut;
                retained += retained_cut;
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

        const double invested_before_buy = shares * close;
        const double base_total = cash + invested_before_buy;
        const double utilization = base_total > 0.0 ? invested_before_buy / base_total : 0.0;

        if (!action_taken && cash > 1e-6 && utilization < cfg.cash_utilization_limit) {
            const double dip_atr = (shares > 1e-9) ? std::max(0.0, (avg_cost - close) / atr) : 0.5;
            double buy_amount = cfg.initial_cash * cfg.base_buy_fraction;
            buy_amount *= (0.60 + 0.25 * static_cast<double>(cfg.aggressiveness));
            if (cfg.buy_sizing_mode == "dynamic") {
                buy_amount *= (1.0 + 0.5 * dip_atr);
            }
            if (cfg.gap_handling_mode == "single_buy") {
                buy_amount = std::min(buy_amount, cash);
            } else {
                buy_amount = std::min(buy_amount * 1.25, cash);
            }
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

        const double equity = cash + taxes + retained + shares * close;
        peak_equity = std::max(peak_equity, equity);
        max_drawdown = std::min(max_drawdown, (equity - peak_equity) / std::max(peak_equity, 1e-9));
        equity_rows.push_back(EquityRow{bar.day, close, cash, taxes, retained, shares, equity});
    }

    const double last_close = bars.empty() ? 0.0 : bars.back().close;
    const double terminal_equity = cash + taxes + retained + shares * last_close;
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
        taxes,
        retained,
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
    os << "  \"used_atr_thresholds\": true,\n";
    os << "  \"used_jump_diffusion\": true,\n";
    os << "  \"buy_sizing_mode\": \"" << cfg.buy_sizing_mode << "\",\n";
    os << "  \"fully_utilized_mode\": \"" << cfg.fully_utilized_mode << "\",\n";
    os << "  \"gap_handling_mode\": \"" << cfg.gap_handling_mode << "\",\n";
    os << "  \"allow_sell_at_loss\": " << (cfg.allow_sell_at_loss ? "true" : "false") << "\n";
    os << "}\n";
    return os.str();
}

}  // namespace cpp_backtester
