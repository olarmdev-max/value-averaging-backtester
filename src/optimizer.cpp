#include "cpp_backtester/optimizer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <dlib/global_optimization.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include "cpp_backtester/io.hpp"
#include "cpp_backtester/monte_carlo.hpp"

namespace cpp_backtester {
namespace {

constexpr double kLongTermTargetLower = 1.0;
constexpr double kLongTermTargetUpper = 8.0;
constexpr double kDailyTargetLower = 0.25;
constexpr double kDailyTargetUpper = 3.0;
constexpr int kAggressivenessLower = 1;
constexpr int kAggressivenessUpper = 4;
constexpr const char* kObjectiveName = "mean_cagr_pct / mean_max_drawdown_pct";

Config make_trial_config(const Config& base, double long_term_target_atr, double daily_target_atr, int aggressiveness) {
    Config trial = base;
    trial.long_term_target_atr = long_term_target_atr;
    trial.daily_target_atr = daily_target_atr;
    trial.aggressiveness = aggressiveness;
    return trial;
}

OptimizationResult optimize_with_dlib(const Config& cfg, const std::vector<PriceSeries>& input_series) {
    OptimizationResult result;
    result.backend_name = "dlib_find_max_global";
    result.objective_name = kObjectiveName;
    result.input_mode = input_series.empty() ? "synthetic_monte_carlo" : "input_files";
    result.input_file_count = static_cast<int>(input_series.size());
    result.requested_evaluations = std::max(1, cfg.optimization_budget);
    result.monte_carlo_sims_per_evaluation = input_series.empty() ? std::max(1, cfg.num_simulations) : 0;
    result.price_files_per_evaluation = input_series.empty() ? 0 : static_cast<int>(input_series.size());
    result.time_budget_seconds = std::max(0.001, cfg.optimization_time_budget_seconds);
    result.best_score = -std::numeric_limits<double>::infinity();
    result.best_config = cfg;
    result.dlib_available = true;

    const auto started_at = std::chrono::steady_clock::now();

    auto objective = [&](double long_term_target_atr, double daily_target_atr, double aggressiveness_continuous) {
        const double clamped_lt = std::clamp(long_term_target_atr, kLongTermTargetLower, kLongTermTargetUpper);
        const double clamped_dt = std::clamp(daily_target_atr, kDailyTargetLower, kDailyTargetUpper);
        const int aggressiveness = std::clamp(static_cast<int>(std::llround(aggressiveness_continuous)), kAggressivenessLower, kAggressivenessUpper);

        const Config trial = make_trial_config(cfg, clamped_lt, clamped_dt, aggressiveness);
        const auto mc = input_series.empty() ? run_monte_carlo(trial) : run_price_series_batch(trial, input_series);
        const double score = mc.cagr_over_drawdown_ratio;

        result.candidates.push_back(OptimizationCandidate{clamped_lt, clamped_dt, aggressiveness, score});
        ++result.evaluated_candidates;
        result.total_monte_carlo_simulations_completed = result.evaluated_candidates * result.monte_carlo_sims_per_evaluation;
        result.total_price_file_evaluations_completed = result.evaluated_candidates * result.price_files_per_evaluation;
        if (score > result.best_score) {
            result.best_score = score;
            result.best_config = trial;
            result.best_mean_total_return_pct = mc.mean_total_return_pct;
            result.best_mean_cagr_pct = mc.mean_cagr_pct;
            result.best_mean_max_drawdown_pct = mc.mean_max_drawdown_pct;
        }
        return score;
    };

    dlib::matrix<double, 0, 1> lower(3);
    dlib::matrix<double, 0, 1> upper(3);
    lower = kLongTermTargetLower, kDailyTargetLower, static_cast<double>(kAggressivenessLower);
    upper = kLongTermTargetUpper, kDailyTargetUpper, static_cast<double>(kAggressivenessUpper);
    const std::vector<bool> is_integer_variable = {false, false, true};

    const auto max_runtime = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(result.time_budget_seconds));

    const auto best_eval = dlib::find_max_global(
        objective,
        lower,
        upper,
        is_integer_variable,
        dlib::max_function_calls(static_cast<std::size_t>(result.requested_evaluations)),
        max_runtime,
        0.05);

    result.elapsed_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at).count();
    result.completed_requested_evaluations = result.evaluated_candidates >= result.requested_evaluations;
    result.stopped_by_time_budget = !result.completed_requested_evaluations && result.elapsed_seconds + 1e-6 >= result.time_budget_seconds;

    if (!std::isfinite(result.best_score)) {
        const double lt = best_eval.x(0);
        const double dt = best_eval.x(1);
        const int aggressiveness = std::clamp(static_cast<int>(std::llround(best_eval.x(2))), kAggressivenessLower, kAggressivenessUpper);
        result.best_config = make_trial_config(cfg, lt, dt, aggressiveness);
        result.best_score = best_eval.y;
    }

    return result;
}

}  // namespace

bool dlib_backend_available() {
    return true;
}

OptimizationResult optimize_config(const Config& cfg, const std::vector<PriceSeries>& input_series) {
    return optimize_with_dlib(cfg, input_series);
}

std::string optimization_result_to_json(const OptimizationResult& result) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "{\n";
    os << "  \"optimizer_backend\": \"" << result.backend_name << "\",\n";
    os << "  \"dlib_available\": " << (result.dlib_available ? "true" : "false") << ",\n";
    os << "  \"objective_name\": \"" << result.objective_name << "\",\n";
    os << "  \"input_mode\": \"" << result.input_mode << "\",\n";
    os << "  \"input_file_count\": " << result.input_file_count << ",\n";
    os << "  \"requested_evaluations\": " << result.requested_evaluations << ",\n";
    os << "  \"evaluated_candidates\": " << result.evaluated_candidates << ",\n";
    os << "  \"monte_carlo_sims_per_evaluation\": " << result.monte_carlo_sims_per_evaluation << ",\n";
    os << "  \"total_monte_carlo_simulations_completed\": " << result.total_monte_carlo_simulations_completed << ",\n";
    os << "  \"price_files_per_evaluation\": " << result.price_files_per_evaluation << ",\n";
    os << "  \"total_price_file_evaluations_completed\": " << result.total_price_file_evaluations_completed << ",\n";
    os << "  \"time_budget_seconds\": " << result.time_budget_seconds << ",\n";
    os << "  \"elapsed_seconds\": " << result.elapsed_seconds << ",\n";
    os << "  \"stopped_by_time_budget\": " << (result.stopped_by_time_budget ? "true" : "false") << ",\n";
    os << "  \"completed_requested_evaluations\": " << (result.completed_requested_evaluations ? "true" : "false") << ",\n";
    os << "  \"best_score\": " << result.best_score << ",\n";
    os << "  \"best_mean_total_return_pct\": " << result.best_mean_total_return_pct << ",\n";
    os << "  \"best_mean_cagr_pct\": " << result.best_mean_cagr_pct << ",\n";
    os << "  \"best_mean_max_drawdown_pct\": " << result.best_mean_max_drawdown_pct << ",\n";
    os << "  \"best_parameters\": {\n";
    os << "    \"long_term_target_atr\": " << result.best_config.long_term_target_atr << ",\n";
    os << "    \"daily_target_atr\": " << result.best_config.daily_target_atr << ",\n";
    os << "    \"aggressiveness\": " << result.best_config.aggressiveness << "\n";
    os << "  },\n";
    os << "  \"candidates\": [\n";
    for (std::size_t i = 0; i < result.candidates.size(); ++i) {
        const auto& candidate = result.candidates[i];
        os << "    {\"long_term_target_atr\": " << candidate.long_term_target_atr
           << ", \"daily_target_atr\": " << candidate.daily_target_atr
           << ", \"aggressiveness\": " << candidate.aggressiveness
           << ", \"score\": " << candidate.score << "}";
        if (i + 1 != result.candidates.size()) {
            os << ',';
        }
        os << "\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os.str();
}

void write_optimization_artifacts(const std::filesystem::path& out_dir, const OptimizationResult& result) {
    write_text(out_dir / "optimization_results.json", optimization_result_to_json(result));
}

}  // namespace cpp_backtester
