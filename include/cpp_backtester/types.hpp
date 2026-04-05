#pragma once

#include <string>
#include <vector>

namespace cpp_backtester {

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

struct SimulationArtifacts {
    std::vector<TradeRow> trades;
    std::vector<EquityRow> equity_rows;
    SimSummary summary;
};

struct McAggregate {
    int simulations = 0;
    double mean_terminal_equity = 0.0;
    double min_terminal_equity = 0.0;
    double max_terminal_equity = 0.0;
    double mean_max_drawdown = 0.0;
    double positive_run_ratio = 0.0;
};

struct OptimizationCandidate {
    double long_term_target_atr = 0.0;
    double daily_target_atr = 0.0;
    int aggressiveness = 0;
    double score = 0.0;
};

struct OptimizationResult {
    std::string backend_name;
    std::string objective;
    int evaluated_candidates = 0;
    double best_score = 0.0;
    Config best_config;
    std::vector<OptimizationCandidate> candidates;
    bool dlib_available = false;
};

}  // namespace cpp_backtester
