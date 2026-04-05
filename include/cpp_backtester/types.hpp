#pragma once

#include <string>
#include <vector>

namespace cpp_backtester {

struct Config {
    int seed = 123;
    int num_days = 252;
    int atr_window = 14;
    int num_simulations = 500;
    int optimization_budget = 100;
    double optimization_time_budget_seconds = 30.0;

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
    int rolling_window_months = 6;
    int rolling_window_step_months = 1;
    int rolling_candidate_pool_size = 36;
    int rolling_max_passes = 0;
    int rolling_worker_count = 0;
    double rolling_total_time_budget_seconds = 30.0;
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

struct PriceSeries {
    std::string name;
    std::string source_path;
    std::vector<std::string> dates;
    std::vector<PriceBar> bars;
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
    double mean_total_return_pct = 0.0;
    double mean_cagr_pct = 0.0;
    double mean_max_drawdown_pct = 0.0;
    double cagr_over_drawdown_ratio = 0.0;
    std::string input_mode = "synthetic_monte_carlo";
    int input_file_count = 0;
};

struct OptimizationCandidate {
    double long_term_target_atr = 0.0;
    double daily_target_atr = 0.0;
    int aggressiveness = 0;
    double score = 0.0;
};

struct OptimizationResult {
    std::string backend_name;
    std::string objective_name;
    std::string input_mode = "synthetic_monte_carlo";
    int input_file_count = 0;
    int requested_evaluations = 0;
    int evaluated_candidates = 0;
    int monte_carlo_sims_per_evaluation = 0;
    int total_monte_carlo_simulations_completed = 0;
    int price_files_per_evaluation = 0;
    int total_price_file_evaluations_completed = 0;
    double time_budget_seconds = 0.0;
    double elapsed_seconds = 0.0;
    double best_score = 0.0;
    double best_mean_total_return_pct = 0.0;
    double best_mean_cagr_pct = 0.0;
    double best_mean_max_drawdown_pct = 0.0;
    Config best_config;
    std::vector<OptimizationCandidate> candidates;
    bool dlib_available = false;
    bool stopped_by_time_budget = false;
    bool completed_requested_evaluations = false;
};

struct RollingWindowBest {
    std::string window_label;
    std::string window_start;
    std::string window_end;
    double long_term_target_atr = 0.0;
    double daily_target_atr = 0.0;
    int aggressiveness = 0;
    double best_score = 0.0;
    double best_mean_total_return_pct = 0.0;
    double best_mean_cagr_pct = 0.0;
    double best_mean_max_drawdown_pct = 0.0;
};

struct RollingReplayRow {
    std::string window_label;
    std::string window_start;
    std::string window_end;
    double mean_total_return_pct = 0.0;
    double mean_cagr_pct = 0.0;
    double mean_max_drawdown_pct = 0.0;
    double cagr_over_drawdown_ratio = 0.0;
};

struct RollingPassSummary {
    int pass_index = 0;
    double elapsed_seconds = 0.0;
    int evaluated_candidates_per_window = 0;
    Config median_config;
    double median_best_score = 0.0;
    double median_best_mean_total_return_pct = 0.0;
    double median_best_mean_cagr_pct = 0.0;
    double median_best_mean_max_drawdown_pct = 0.0;
    std::vector<RollingWindowBest> window_bests;
};

struct RollingOptimizationResult {
    std::string objective_name;
    std::string input_mode = "dated_input_files";
    int input_file_count = 0;
    std::string common_start;
    std::string common_end;
    int window_count = 0;
    int window_months = 0;
    int window_step_months = 0;
    int candidate_pool_size = 0;
    int worker_count = 0;
    int max_passes = 0;
    int passes_completed = 0;
    int total_candidate_evaluations_completed = 0;
    double total_time_budget_seconds = 0.0;
    double elapsed_seconds_total = 0.0;
    bool stopped_by_time_budget = false;
    Config final_config;
    double final_mean_total_return_pct = 0.0;
    double final_mean_cagr_pct = 0.0;
    double final_mean_max_drawdown_pct = 0.0;
    double final_cagr_over_drawdown_ratio = 0.0;
    std::vector<RollingPassSummary> pass_summaries;
    std::vector<RollingReplayRow> replay_rows;
};

}  // namespace cpp_backtester
