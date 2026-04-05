#include "cpp_backtester/optimizer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#ifdef CPP_BACKTESTER_HAS_DLIB
#include <dlib/global_optimization.h>
#endif

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

Config make_trial_config(const Config& base, double long_term_target_atr, double daily_target_atr, int aggressiveness) {
    Config trial = base;
    trial.long_term_target_atr = long_term_target_atr;
    trial.daily_target_atr = daily_target_atr;
    trial.aggressiveness = aggressiveness;
    trial.num_simulations = base.optimization_mc_sims;
    return trial;
}

double compute_objective_score(const Config& cfg, const McAggregate& mc) {
    if (cfg.objective == "positive_run_ratio") {
        return mc.positive_run_ratio;
    }

    if (cfg.objective == "drawdown_penalized_equity") {
        const double penalty = std::max(0.0, 1.0 + mc.mean_max_drawdown);
        return mc.mean_terminal_equity * penalty;
    }

    if (cfg.objective == "risk_adjusted_return") {
        const double normalized_return = (mc.mean_terminal_equity - cfg.initial_cash) / std::max(1.0, cfg.initial_cash);
        return normalized_return / (1.0 + std::abs(mc.mean_max_drawdown));
    }

    return mc.mean_terminal_equity;
}

#ifndef CPP_BACKTESTER_HAS_DLIB
OptimizationResult optimize_with_grid_placeholder(const Config& cfg) {
    const std::vector<double> long_targets{2.0, 3.0, 4.0, 5.0, 6.0};
    const std::vector<double> daily_targets{0.5, 0.75, 1.0, 1.25, 1.5};
    const std::vector<int> aggressiveness_values{1, 2, 3, 4};

    OptimizationResult result;
    result.backend_name = "grid-smoke-placeholder";
    result.objective = cfg.objective;
    result.best_score = -std::numeric_limits<double>::infinity();
    result.best_config = cfg;
    result.dlib_available = false;

    for (double lt : long_targets) {
        for (double dt : daily_targets) {
            for (int aggr : aggressiveness_values) {
                if (result.evaluated_candidates >= cfg.optimization_budget) {
                    return result;
                }

                const Config trial = make_trial_config(cfg, lt, dt, aggr);
                const auto mc = run_monte_carlo(trial);
                const double score = compute_objective_score(trial, mc);

                result.candidates.push_back(OptimizationCandidate{lt, dt, aggr, score});
                ++result.evaluated_candidates;
                if (score > result.best_score) {
                    result.best_score = score;
                    result.best_config = trial;
                }
            }
        }
    }

    return result;
}
#endif

#ifdef CPP_BACKTESTER_HAS_DLIB
OptimizationResult optimize_with_dlib(const Config& cfg) {
    OptimizationResult result;
    result.backend_name = "dlib_find_max_global";
    result.objective = cfg.objective;
    result.best_score = -std::numeric_limits<double>::infinity();
    result.best_config = cfg;
    result.dlib_available = true;

    auto objective = [&](double long_term_target_atr, double daily_target_atr, double aggressiveness_continuous) {
        const double clamped_lt = std::clamp(long_term_target_atr, kLongTermTargetLower, kLongTermTargetUpper);
        const double clamped_dt = std::clamp(daily_target_atr, kDailyTargetLower, kDailyTargetUpper);
        const int aggressiveness = std::clamp(static_cast<int>(std::llround(aggressiveness_continuous)), kAggressivenessLower, kAggressivenessUpper);

        const Config trial = make_trial_config(cfg, clamped_lt, clamped_dt, aggressiveness);
        const auto mc = run_monte_carlo(trial);
        const double score = compute_objective_score(trial, mc);

        result.candidates.push_back(OptimizationCandidate{clamped_lt, clamped_dt, aggressiveness, score});
        ++result.evaluated_candidates;
        if (score > result.best_score) {
            result.best_score = score;
            result.best_config = trial;
        }
        return score;
    };

    dlib::matrix<double, 0, 1> lower(3);
    dlib::matrix<double, 0, 1> upper(3);
    lower = kLongTermTargetLower, kDailyTargetLower, static_cast<double>(kAggressivenessLower);
    upper = kLongTermTargetUpper, kDailyTargetUpper, static_cast<double>(kAggressivenessUpper);
    const std::vector<bool> is_integer_variable = {false, false, true};

    const auto best_eval = dlib::find_max_global(
        objective,
        lower,
        upper,
        is_integer_variable,
        dlib::max_function_calls(static_cast<std::size_t>(std::max(1, cfg.optimization_budget))),
        std::chrono::seconds(45),
        0.05);

    if (!std::isfinite(result.best_score)) {
        const double lt = best_eval.x(0);
        const double dt = best_eval.x(1);
        const int aggressiveness = std::clamp(static_cast<int>(std::llround(best_eval.x(2))), kAggressivenessLower, kAggressivenessUpper);
        result.best_config = make_trial_config(cfg, lt, dt, aggressiveness);
        result.best_score = best_eval.y;
    }

    return result;
}
#endif

}  // namespace

bool dlib_backend_available() {
#ifdef CPP_BACKTESTER_HAS_DLIB
    return true;
#else
    return false;
#endif
}

OptimizationResult optimize_config(const Config& cfg) {
#ifdef CPP_BACKTESTER_HAS_DLIB
    return optimize_with_dlib(cfg);
#else
    return optimize_with_grid_placeholder(cfg);
#endif
}

std::string optimization_result_to_json(const OptimizationResult& result) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "{\n";
    os << "  \"optimizer_backend\": \"" << result.backend_name << "\",\n";
    os << "  \"dlib_available\": " << (result.dlib_available ? "true" : "false") << ",\n";
    os << "  \"objective\": \"" << result.objective << "\",\n";
    os << "  \"evaluated_candidates\": " << result.evaluated_candidates << ",\n";
    os << "  \"best_score\": " << result.best_score << ",\n";
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
