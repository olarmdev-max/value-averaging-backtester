#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Config {
    int seed = 123;
    int num_days = 252;
    int atr_window = 14;
    int num_simulations = 500;
    int optimization_budget = 12;
    int optimization_mc_sims = 24;

    double start_price = 100.0;
    double mu = 0.08;
    double sigma = 0.30;
    double jump_probability = 0.02;
    double jump_mean = -0.03;
    double jump_std = 0.08;

    double initial_cash = 10000.0;
    double long_term_target_atr = 4.0;
    double daily_target_atr = 1.0;
    double cash_utilization_limit = 0.95;
    int aggressiveness = 2;
    double base_buy_fraction = 0.05;
    std::string buy_sizing_mode = "dynamic";
    double taxes_retained_pct = 0.25;
    double profit_retained_pct = 0.15;
    bool allow_sell_at_loss = false;
    bool one_buy_per_day_only = true;
    std::string fully_utilized_mode = "wait";
    std::string gap_handling_mode = "single_buy";
    std::string objective = "mean_terminal_equity";
};

struct PriceBar {
    int day = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double atr = 0.0;
    bool jump_applied = false;
};

struct TradeRow {
    int day = 0;
    std::string action;
    double qty = 0.0;
    double price = 0.0;
    double cash_after = 0.0;
    double shares_after = 0.0;
    double gross_profit = 0.0;
};

struct EquityRow {
    int day = 0;
    double close = 0.0;
    double cash = 0.0;
    double taxes = 0.0;
    double retained = 0.0;
    double shares = 0.0;
    double equity = 0.0;
};

struct SimSummary {
    int num_days = 0;
    int buy_count = 0;
    int sell_count = 0;
    int reset_sell_count = 0;
    int overage_sell_count = 0;
    double terminal_equity = 0.0;
    double terminal_cash = 0.0;
    double terminal_shares = 0.0;
    double taxes_retained = 0.0;
    double profit_retained = 0.0;
    double max_drawdown = 0.0;
    double utilization_final = 0.0;
};

struct McAggregate {
    int simulations = 0;
    double mean_terminal_equity = 0.0;
    double min_terminal_equity = 0.0;
    double max_terminal_equity = 0.0;
    double mean_max_drawdown = 0.0;
    double positive_run_ratio = 0.0;
};

std::string read_file(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void write_text(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write file: " + path.string());
    }
    out << content;
}

double extract_double(const std::string& text, const std::string& key, double default_value) {
    std::regex rgx("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(text, match, rgx)) {
        return std::stod(match[1].str());
    }
    return default_value;
}

int extract_int(const std::string& text, const std::string& key, int default_value) {
    std::regex rgx("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (std::regex_search(text, match, rgx)) {
        return std::stoi(match[1].str());
    }
    return default_value;
}

bool extract_bool(const std::string& text, const std::string& key, bool default_value) {
    std::regex rgx("\\\"" + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(text, match, rgx)) {
        return match[1].str() == "true";
    }
    return default_value;
}

std::string extract_string(const std::string& text, const std::string& key, const std::string& default_value) {
    std::regex rgx("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (std::regex_search(text, match, rgx)) {
        return match[1].str();
    }
    return default_value;
}

Config load_config(const fs::path& path) {
    const std::string text = read_file(path);
    Config cfg;
    cfg.seed = extract_int(text, "seed", cfg.seed);
    cfg.num_days = extract_int(text, "num_days", cfg.num_days);
    cfg.atr_window = extract_int(text, "atr_window", cfg.atr_window);
    cfg.num_simulations = extract_int(text, "num_simulations", cfg.num_simulations);
    cfg.optimization_budget = extract_int(text, "optimization_budget", cfg.optimization_budget);
    cfg.optimization_mc_sims = extract_int(text, "optimization_mc_sims", cfg.optimization_mc_sims);

    cfg.start_price = extract_double(text, "start_price", cfg.start_price);
    cfg.mu = extract_double(text, "mu", cfg.mu);
    cfg.sigma = extract_double(text, "sigma", cfg.sigma);
    cfg.jump_probability = extract_double(text, "jump_probability", cfg.jump_probability);
    cfg.jump_mean = extract_double(text, "jump_mean", cfg.jump_mean);
    cfg.jump_std = extract_double(text, "jump_std", cfg.jump_std);

    cfg.initial_cash = extract_double(text, "initial_cash", cfg.initial_cash);
    cfg.long_term_target_atr = extract_double(text, "long_term_target_atr", cfg.long_term_target_atr);
    cfg.daily_target_atr = extract_double(text, "daily_target_atr", cfg.daily_target_atr);
    cfg.cash_utilization_limit = extract_double(text, "cash_utilization_limit", cfg.cash_utilization_limit);
    cfg.aggressiveness = extract_int(text, "aggressiveness", cfg.aggressiveness);
    cfg.base_buy_fraction = extract_double(text, "base_buy_fraction", cfg.base_buy_fraction);
    cfg.buy_sizing_mode = extract_string(text, "buy_sizing_mode", cfg.buy_sizing_mode);
    cfg.taxes_retained_pct = extract_double(text, "taxes_retained_pct", cfg.taxes_retained_pct);
    cfg.profit_retained_pct = extract_double(text, "profit_retained_pct", cfg.profit_retained_pct);
    cfg.allow_sell_at_loss = extract_bool(text, "allow_sell_at_loss", cfg.allow_sell_at_loss);
    cfg.one_buy_per_day_only = extract_bool(text, "one_buy_per_day_only", cfg.one_buy_per_day_only);
    cfg.fully_utilized_mode = extract_string(text, "fully_utilized_mode", cfg.fully_utilized_mode);
    cfg.gap_handling_mode = extract_string(text, "gap_handling_mode", cfg.gap_handling_mode);
    cfg.objective = extract_string(text, "objective", cfg.objective);
    return cfg;
}

std::vector<PriceBar> generate_prices(const Config& cfg, int seed_offset = 0) {
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

void write_prices_csv(const fs::path& out_dir, const std::vector<PriceBar>& bars) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "day,open,high,low,close,atr,jump_applied\n";
    for (const auto& bar : bars) {
        os << bar.day << ',' << bar.open << ',' << bar.high << ',' << bar.low << ',' << bar.close << ',' << bar.atr << ',' << (bar.jump_applied ? 1 : 0) << "\n";
    }
    write_text(out_dir / "prices.csv", os.str());
}

SimSummary simulate_path(const Config& cfg, const std::vector<PriceBar>& bars, const fs::path& out_dir, bool write_artifacts) {
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
            const double dip_atr = (shares > 1e-9) ? std::max(0.0, (avg_cost - close) / std::max(bar.atr, 1e-6)) : 0.5;
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

    if (write_artifacts) {
        std::ostringstream trades_csv;
        trades_csv << std::fixed << std::setprecision(6);
        trades_csv << "day,action,qty,price,cash_after,shares_after,gross_profit\n";
        for (const auto& row : trades) {
            trades_csv << row.day << ',' << row.action << ',' << row.qty << ',' << row.price << ',' << row.cash_after << ',' << row.shares_after << ',' << row.gross_profit << "\n";
        }
        write_text(out_dir / "trade_log.csv", trades_csv.str());

        std::ostringstream equity_csv;
        equity_csv << std::fixed << std::setprecision(6);
        equity_csv << "day,close,cash,taxes,retained,shares,equity\n";
        for (const auto& row : equity_rows) {
            equity_csv << row.day << ',' << row.close << ',' << row.cash << ',' << row.taxes << ',' << row.retained << ',' << row.shares << ',' << row.equity << "\n";
        }
        write_text(out_dir / "equity.csv", equity_csv.str());
    }

    const double last_close = bars.empty() ? 0.0 : bars.back().close;
    const double terminal_equity = cash + taxes + retained + shares * last_close;
    const double investable_total = cash + shares * last_close;
    const double utilization_final = investable_total > 0.0 ? (shares * last_close) / investable_total : 0.0;

    return SimSummary{
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
        utilization_final
    };
}

std::string summary_json(const Config& cfg, const SimSummary& summary) {
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

McAggregate run_monte_carlo(const Config& cfg) {
    std::vector<double> terminal_equities;
    std::vector<double> drawdowns;
    terminal_equities.reserve(static_cast<std::size_t>(cfg.num_simulations));
    drawdowns.reserve(static_cast<std::size_t>(cfg.num_simulations));

    for (int i = 0; i < cfg.num_simulations; ++i) {
        const auto bars = generate_prices(cfg, i + 1);
        const auto summary = simulate_path(cfg, bars, {}, false);
        terminal_equities.push_back(summary.terminal_equity);
        drawdowns.push_back(summary.max_drawdown);
    }

    const double mean_equity = std::accumulate(terminal_equities.begin(), terminal_equities.end(), 0.0) / std::max<std::size_t>(1, terminal_equities.size());
    const double mean_dd = std::accumulate(drawdowns.begin(), drawdowns.end(), 0.0) / std::max<std::size_t>(1, drawdowns.size());
    const auto [min_it, max_it] = std::minmax_element(terminal_equities.begin(), terminal_equities.end());
    const int positives = static_cast<int>(std::count_if(terminal_equities.begin(), terminal_equities.end(), [&](double x) { return x > cfg.initial_cash; }));

    return McAggregate{
        cfg.num_simulations,
        mean_equity,
        terminal_equities.empty() ? 0.0 : *min_it,
        terminal_equities.empty() ? 0.0 : *max_it,
        mean_dd,
        cfg.num_simulations > 0 ? static_cast<double>(positives) / static_cast<double>(cfg.num_simulations) : 0.0
    };
}

std::string mc_json(const Config& cfg, const McAggregate& mc) {
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

std::string optimize(const Config& cfg, const fs::path& out_dir) {
    const std::vector<double> long_targets{2.0, 3.0, 4.0, 5.0, 6.0};
    const std::vector<double> daily_targets{0.5, 0.75, 1.0, 1.25, 1.5};
    const std::vector<int> aggressiveness_values{1, 2, 3, 4};

    int evaluated = 0;
    double best_score = -1e100;
    Config best_cfg = cfg;
    std::ostringstream candidates;
    candidates << std::fixed << std::setprecision(6);
    candidates << "[\n";

    for (double lt : long_targets) {
        for (double dt : daily_targets) {
            for (int aggr : aggressiveness_values) {
                if (evaluated >= cfg.optimization_budget) {
                    break;
                }
                Config trial = cfg;
                trial.long_term_target_atr = lt;
                trial.daily_target_atr = dt;
                trial.aggressiveness = aggr;
                trial.num_simulations = cfg.optimization_mc_sims;
                const auto mc = run_monte_carlo(trial);
                const double score = mc.mean_terminal_equity;
                if (score > best_score) {
                    best_score = score;
                    best_cfg = trial;
                }
                candidates << "  {\"long_term_target_atr\": " << lt
                           << ", \"daily_target_atr\": " << dt
                           << ", \"aggressiveness\": " << aggr
                           << ", \"score\": " << score << "}";
                ++evaluated;
                if (evaluated < cfg.optimization_budget) {
                    candidates << ',';
                }
                candidates << "\n";
            }
            if (evaluated >= cfg.optimization_budget) {
                break;
            }
        }
        if (evaluated >= cfg.optimization_budget) {
            break;
        }
    }
    candidates << "]";

    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "{\n";
    os << "  \"optimizer_mode\": \"smoke_grid_placeholder_for_future_bayesian_optimizer\",\n";
    os << "  \"objective\": \"" << cfg.objective << "\",\n";
    os << "  \"evaluated_candidates\": " << evaluated << ",\n";
    os << "  \"best_score\": " << best_score << ",\n";
    os << "  \"best_parameters\": {\n";
    os << "    \"long_term_target_atr\": " << best_cfg.long_term_target_atr << ",\n";
    os << "    \"daily_target_atr\": " << best_cfg.daily_target_atr << ",\n";
    os << "    \"aggressiveness\": " << best_cfg.aggressiveness << "\n";
    os << "  },\n";
    os << "  \"candidates\": " << candidates.str() << "\n";
    os << "}\n";
    write_text(out_dir / "optimization_results.json", os.str());
    return os.str();
}

int main(int argc, char** argv) {
    try {
        if (argc < 4) {
            std::cerr << "Usage: cpp_backtester <generate|simulate|monte-carlo|optimize> <config.json> <output_dir>\n";
            return 1;
        }

        const std::string command = argv[1];
        const fs::path config_path = argv[2];
        const fs::path out_dir = argv[3];
        fs::create_directories(out_dir);

        const Config cfg = load_config(config_path);

        if (command == "generate") {
            const auto bars = generate_prices(cfg);
            write_prices_csv(out_dir, bars);
            return 0;
        }

        if (command == "simulate") {
            const auto bars = generate_prices(cfg);
            write_prices_csv(out_dir, bars);
            const auto summary = simulate_path(cfg, bars, out_dir, true);
            write_text(out_dir / "summary.json", summary_json(cfg, summary));
            return 0;
        }

        if (command == "monte-carlo") {
            const auto mc = run_monte_carlo(cfg);
            write_text(out_dir / "aggregate.json", mc_json(cfg, mc));
            return 0;
        }

        if (command == "optimize") {
            optimize(cfg, out_dir);
            return 0;
        }

        std::cerr << "Unknown command: " << command << '\n';
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
