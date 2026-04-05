#include "cpp_backtester/optimizer.hpp"

#include <sstream>

#include "cpp_backtester/io.hpp"
#include "cpp_backtester/monte_carlo.hpp"

namespace cpp_backtester {

bool dlib_backend_available() {
#ifdef CPP_BACKTESTER_HAS_DLIB
    return true;
#else
    return false;
#endif
}

OptimizationResult optimize_config(const Config& cfg) {
    const std::vector<double> long_targets{2.0, 3.0, 4.0, 5.0, 6.0};
    const std::vector<double> daily_targets{0.5, 0.75, 1.0, 1.25, 1.5};
    const std::vector<int> aggressiveness_values{1, 2, 3, 4};

    OptimizationResult result;
    result.backend_name = dlib_backend_available() ? "dlib-ready-placeholder" : "grid-smoke-placeholder";
    result.objective = cfg.objective;
    result.best_score = -1e100;
    result.best_config = cfg;
    result.dlib_available = dlib_backend_available();

    for (double lt : long_targets) {
        for (double dt : daily_targets) {
            for (int aggr : aggressiveness_values) {
                if (result.evaluated_candidates >= cfg.optimization_budget) {
                    return result;
                }

                Config trial = cfg;
                trial.long_term_target_atr = lt;
                trial.daily_target_atr = dt;
                trial.aggressiveness = aggr;
                trial.num_simulations = cfg.optimization_mc_sims;
                const auto mc = run_monte_carlo(trial);
                const double score = mc.mean_terminal_equity;

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
