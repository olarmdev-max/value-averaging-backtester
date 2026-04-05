#include "cpp_backtester/rolling_optimizer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
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

struct CalendarDate {
    int year = 0;
    int month = 0;
    int day = 0;
};

struct DatedSeries {
    PriceSeries series;
    std::vector<CalendarDate> parsed_dates;
};

struct WindowInput {
    std::string label;
    std::string start_text;
    std::string end_text;
    std::vector<PriceSeries> series_list;
};

bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(int year, int month) {
    static const int kDaysByMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        throw std::runtime_error("Invalid month value in date handling.");
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return kDaysByMonth[month - 1];
}

CalendarDate parse_calendar_date(const std::string& text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        throw std::runtime_error("Expected ISO date YYYY-MM-DD, got: " + text);
    }
    const int year = std::stoi(text.substr(0, 4));
    const int month = std::stoi(text.substr(5, 2));
    const int day = std::stoi(text.substr(8, 2));
    if (month < 1 || month > 12) {
        throw std::runtime_error("Invalid month in date: " + text);
    }
    if (day < 1 || day > days_in_month(year, month)) {
        throw std::runtime_error("Invalid day in date: " + text);
    }
    return CalendarDate{year, month, day};
}

std::string calendar_date_to_string(const CalendarDate& date) {
    std::ostringstream os;
    os << std::setfill('0')
       << std::setw(4) << date.year << '-'
       << std::setw(2) << date.month << '-'
       << std::setw(2) << date.day;
    return os.str();
}

bool operator<(const CalendarDate& lhs, const CalendarDate& rhs) {
    if (lhs.year != rhs.year) {
        return lhs.year < rhs.year;
    }
    if (lhs.month != rhs.month) {
        return lhs.month < rhs.month;
    }
    return lhs.day < rhs.day;
}

CalendarDate add_months(const CalendarDate& date, int delta_months) {
    const int absolute_month = (date.year * 12) + (date.month - 1) + delta_months;
    const int year = absolute_month / 12;
    const int month = (absolute_month % 12 + 12) % 12 + 1;
    const int adjusted_year = absolute_month >= 0 || absolute_month % 12 == 0 ? year : year - 1;
    const int day = std::min(date.day, days_in_month(adjusted_year, month));
    return CalendarDate{adjusted_year, month, day};
}

double median_double(std::vector<double> values) {
    if (values.empty()) {
        throw std::runtime_error("Cannot take median of empty value set.");
    }
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    if (values.size() % 2 == 1) {
        return values[mid];
    }
    return 0.5 * (values[mid - 1] + values[mid]);
}

int median_int_round_half_up(std::vector<int> values) {
    std::vector<double> as_double;
    as_double.reserve(values.size());
    for (int value : values) {
        as_double.push_back(static_cast<double>(value));
    }
    return std::clamp(static_cast<int>(std::floor(median_double(std::move(as_double)) + 0.5)), kAggressivenessLower, kAggressivenessUpper);
}

Config make_trial_config(const Config& base, double long_term_target_atr, double daily_target_atr, int aggressiveness) {
    Config trial = base;
    trial.long_term_target_atr = std::clamp(long_term_target_atr, kLongTermTargetLower, kLongTermTargetUpper);
    trial.daily_target_atr = std::clamp(daily_target_atr, kDailyTargetLower, kDailyTargetUpper);
    trial.aggressiveness = std::clamp(aggressiveness, kAggressivenessLower, kAggressivenessUpper);
    return trial;
}

Config median_config_from_window_bests(const Config& base, const std::vector<RollingWindowBest>& window_bests) {
    std::vector<double> long_terms;
    std::vector<double> daily_targets;
    std::vector<int> aggressiveness_values;
    long_terms.reserve(window_bests.size());
    daily_targets.reserve(window_bests.size());
    aggressiveness_values.reserve(window_bests.size());

    for (const auto& best : window_bests) {
        long_terms.push_back(best.long_term_target_atr);
        daily_targets.push_back(best.daily_target_atr);
        aggressiveness_values.push_back(best.aggressiveness);
    }

    return make_trial_config(
        base,
        median_double(std::move(long_terms)),
        median_double(std::move(daily_targets)),
        median_int_round_half_up(std::move(aggressiveness_values)));
}

std::vector<DatedSeries> require_dated_series(const std::vector<PriceSeries>& input_series) {
    if (input_series.empty()) {
        throw std::runtime_error("Rolling optimization requires one or more dated CSV input files.");
    }

    std::vector<DatedSeries> dated_series;
    dated_series.reserve(input_series.size());
    for (const auto& series : input_series) {
        if (series.dates.empty()) {
            throw std::runtime_error(
                "Rolling optimization requires YYYY-MM-DD date columns in every input CSV. Missing dates in: " + series.source_path);
        }
        if (series.dates.size() != series.bars.size()) {
            throw std::runtime_error("Loaded date count does not match bar count for: " + series.source_path);
        }

        DatedSeries dated{series, {}};
        dated.parsed_dates.reserve(series.dates.size());
        CalendarDate previous;
        bool has_previous = false;
        for (const auto& date_text : series.dates) {
            const CalendarDate parsed = parse_calendar_date(date_text);
            if (has_previous && parsed < previous) {
                throw std::runtime_error("Input CSV dates must be sorted ascending: " + series.source_path);
            }
            dated.parsed_dates.push_back(parsed);
            previous = parsed;
            has_previous = true;
        }
        dated_series.push_back(std::move(dated));
    }
    return dated_series;
}

std::vector<WindowInput> build_window_inputs(
    const std::vector<DatedSeries>& dated_series,
    const Config& cfg,
    std::string* common_start_text,
    std::string* common_end_text) {
    if (cfg.rolling_window_months <= 0) {
        throw std::runtime_error("rolling_window_months must be >= 1.");
    }
    if (cfg.rolling_window_step_months <= 0) {
        throw std::runtime_error("rolling_window_step_months must be >= 1.");
    }

    CalendarDate common_start = dated_series.front().parsed_dates.front();
    CalendarDate common_end = dated_series.front().parsed_dates.back();
    for (const auto& series : dated_series) {
        common_start = std::max(common_start, series.parsed_dates.front());
        common_end = std::min(common_end, series.parsed_dates.back());
    }
    if (common_end < common_start) {
        throw std::runtime_error("Input dated CSV files do not share any overlapping date range.");
    }

    *common_start_text = calendar_date_to_string(common_start);
    *common_end_text = calendar_date_to_string(common_end);

    std::vector<WindowInput> windows;
    CalendarDate window_end = common_end;
    while (true) {
        const CalendarDate window_start = add_months(window_end, -cfg.rolling_window_months);
        if (window_start < common_start) {
            break;
        }

        WindowInput window;
        window.start_text = calendar_date_to_string(window_start);
        window.end_text = calendar_date_to_string(window_end);
        window.label = window.start_text + "__" + window.end_text;
        window.series_list.reserve(dated_series.size());

        for (const auto& series : dated_series) {
            auto first_it = std::lower_bound(series.parsed_dates.begin(), series.parsed_dates.end(), window_start);
            auto last_it = std::upper_bound(series.parsed_dates.begin(), series.parsed_dates.end(), window_end);
            if (first_it == last_it) {
                throw std::runtime_error(
                    "A rolling window produced an empty slice for input: " + series.series.source_path +
                    " in window " + window.label);
            }

            const std::size_t first_index = static_cast<std::size_t>(std::distance(series.parsed_dates.begin(), first_it));
            const std::size_t last_index = static_cast<std::size_t>(std::distance(series.parsed_dates.begin(), last_it));

            PriceSeries sliced;
            sliced.name = series.series.name;
            sliced.source_path = series.series.source_path;
            sliced.dates.assign(series.series.dates.begin() + static_cast<std::ptrdiff_t>(first_index),
                                series.series.dates.begin() + static_cast<std::ptrdiff_t>(last_index));
            sliced.bars.assign(series.series.bars.begin() + static_cast<std::ptrdiff_t>(first_index),
                               series.series.bars.begin() + static_cast<std::ptrdiff_t>(last_index));
            window.series_list.push_back(std::move(sliced));
        }

        windows.push_back(std::move(window));
        window_end = add_months(window_end, -cfg.rolling_window_step_months);
    }

    if (windows.empty()) {
        throw std::runtime_error(
            "No rolling windows fit inside the shared dated range. Increase input history or reduce rolling_window_months.");
    }
    return windows;
}

int resolve_worker_count(int configured_worker_count, std::size_t window_count) {
    const unsigned int detected = std::thread::hardware_concurrency();
    const int base_workers = configured_worker_count > 0
        ? configured_worker_count
        : static_cast<int>(detected == 0 ? 1U : detected);
    return std::max(1, std::min(base_workers, static_cast<int>(std::max<std::size_t>(1, window_count))));
}

std::vector<OptimizationCandidate> build_candidate_pool(
    const Config& center,
    int pass_index,
    int candidate_pool_size,
    std::mt19937& rng) {
    if (candidate_pool_size <= 0) {
        throw std::runtime_error("rolling_candidate_pool_size must be >= 1.");
    }

    std::vector<OptimizationCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(candidate_pool_size));
    candidates.push_back(OptimizationCandidate{
        center.long_term_target_atr,
        center.daily_target_atr,
        center.aggressiveness,
        0.0});

    std::uniform_real_distribution<double> long_term_dist(kLongTermTargetLower, kLongTermTargetUpper);
    std::uniform_real_distribution<double> daily_dist(kDailyTargetLower, kDailyTargetUpper);
    std::uniform_int_distribution<int> aggressiveness_dist(kAggressivenessLower, kAggressivenessUpper);
    std::uniform_int_distribution<int> delta_aggressiveness_dist(-1, 1);

    for (int i = 1; i < candidate_pool_size; ++i) {
        if (pass_index == 1) {
            candidates.push_back(OptimizationCandidate{
                long_term_dist(rng),
                daily_dist(rng),
                aggressiveness_dist(rng),
                0.0});
            continue;
        }

        const double long_radius = 1.5 / static_cast<double>(pass_index - 1);
        const double daily_radius = 0.6 / static_cast<double>(pass_index - 1);
        std::uniform_real_distribution<double> long_local_dist(
            std::max(kLongTermTargetLower, center.long_term_target_atr - long_radius),
            std::min(kLongTermTargetUpper, center.long_term_target_atr + long_radius));
        std::uniform_real_distribution<double> daily_local_dist(
            std::max(kDailyTargetLower, center.daily_target_atr - daily_radius),
            std::min(kDailyTargetUpper, center.daily_target_atr + daily_radius));

        candidates.push_back(OptimizationCandidate{
            long_local_dist(rng),
            daily_local_dist(rng),
            std::clamp(center.aggressiveness + delta_aggressiveness_dist(rng), kAggressivenessLower, kAggressivenessUpper),
            0.0});
    }

    return candidates;
}

template <typename ResultType, typename WorkerFunc>
std::vector<ResultType> parallel_collect(std::size_t item_count, int worker_count, WorkerFunc worker_func) {
    std::vector<ResultType> results(item_count);
    std::atomic<std::size_t> next_index{0};
    std::exception_ptr worker_error;
    std::mutex error_mutex;
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(worker_count));

    for (int worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&]() {
            while (true) {
                {
                    std::lock_guard<std::mutex> lock(error_mutex);
                    if (worker_error) {
                        return;
                    }
                }

                const std::size_t index = next_index.fetch_add(1);
                if (index >= item_count) {
                    return;
                }

                try {
                    results[index] = worker_func(index);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(error_mutex);
                    if (!worker_error) {
                        worker_error = std::current_exception();
                    }
                    return;
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    if (worker_error) {
        std::rethrow_exception(worker_error);
    }
    return results;
}

RollingWindowBest evaluate_window_candidates(
    const WindowInput& window,
    const Config& base_cfg,
    const std::vector<OptimizationCandidate>& candidates) {
    RollingWindowBest best;
    best.window_label = window.label;
    best.window_start = window.start_text;
    best.window_end = window.end_text;
    best.best_score = -std::numeric_limits<double>::infinity();

    for (const auto& candidate : candidates) {
        const Config trial = make_trial_config(
            base_cfg,
            candidate.long_term_target_atr,
            candidate.daily_target_atr,
            candidate.aggressiveness);
        const auto aggregate = run_price_series_batch(trial, window.series_list);
        const double score = aggregate.cagr_over_drawdown_ratio;

        if (score > best.best_score) {
            best.long_term_target_atr = trial.long_term_target_atr;
            best.daily_target_atr = trial.daily_target_atr;
            best.aggressiveness = trial.aggressiveness;
            best.best_score = score;
            best.best_mean_total_return_pct = aggregate.mean_total_return_pct;
            best.best_mean_cagr_pct = aggregate.mean_cagr_pct;
            best.best_mean_max_drawdown_pct = aggregate.mean_max_drawdown_pct;
        }
    }

    if (!std::isfinite(best.best_score)) {
        throw std::runtime_error("No valid candidate score produced for rolling window: " + window.label);
    }
    return best;
}

RollingReplayRow replay_window(const WindowInput& window, const Config& final_cfg) {
    const auto aggregate = run_price_series_batch(final_cfg, window.series_list);
    return RollingReplayRow{
        window.label,
        window.start_text,
        window.end_text,
        aggregate.mean_total_return_pct,
        aggregate.mean_cagr_pct,
        aggregate.mean_max_drawdown_pct,
        aggregate.cagr_over_drawdown_ratio,
    };
}

std::string params_to_json(const Config& cfg, int indent_spaces) {
    const std::string indent(static_cast<std::size_t>(indent_spaces), ' ');
    const std::string next_indent(static_cast<std::size_t>(indent_spaces + 2), ' ');
    std::ostringstream os;
    os << indent << "{\n";
    os << next_indent << "\"long_term_target_atr\": " << cfg.long_term_target_atr << ",\n";
    os << next_indent << "\"daily_target_atr\": " << cfg.daily_target_atr << ",\n";
    os << next_indent << "\"aggressiveness\": " << cfg.aggressiveness << "\n";
    os << indent << "}";
    return os.str();
}

}  // namespace

RollingOptimizationResult optimize_rolling_windows(const Config& cfg, const std::vector<PriceSeries>& input_series) {
    if (cfg.rolling_total_time_budget_seconds <= 0.0 && cfg.rolling_max_passes <= 0) {
        throw std::runtime_error(
            "optimize-rolling needs a stop condition. Set rolling_total_time_budget_seconds > 0 or rolling_max_passes > 0.");
    }

    const auto dated_series = require_dated_series(input_series);

    RollingOptimizationResult result;
    result.objective_name = kObjectiveName;
    result.input_file_count = static_cast<int>(input_series.size());
    result.window_months = cfg.rolling_window_months;
    result.window_step_months = cfg.rolling_window_step_months;
    result.candidate_pool_size = cfg.rolling_candidate_pool_size;
    result.max_passes = cfg.rolling_max_passes;
    result.total_time_budget_seconds = std::max(0.0, cfg.rolling_total_time_budget_seconds);

    std::string common_start_text;
    std::string common_end_text;
    const auto windows = build_window_inputs(dated_series, cfg, &common_start_text, &common_end_text);
    result.common_start = common_start_text;
    result.common_end = common_end_text;
    result.window_count = static_cast<int>(windows.size());
    result.worker_count = resolve_worker_count(cfg.rolling_worker_count, windows.size());

    const auto started_at = std::chrono::steady_clock::now();
    std::mt19937 rng(static_cast<std::mt19937::result_type>(cfg.seed));
    Config current_center = cfg;
    double last_pass_elapsed_seconds = 0.0;

    for (int pass_index = 1;; ++pass_index) {
        if (result.max_passes > 0 && pass_index > result.max_passes) {
            break;
        }

        if (result.total_time_budget_seconds > 0.0 && pass_index > 1) {
            const double elapsed_so_far = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at).count();
            const double remaining_seconds = result.total_time_budget_seconds - elapsed_so_far;
            if (remaining_seconds <= 0.0 || (last_pass_elapsed_seconds > 0.0 && remaining_seconds + 1e-9 < last_pass_elapsed_seconds)) {
                result.stopped_by_time_budget = true;
                break;
            }
        }

        const auto candidates = build_candidate_pool(current_center, pass_index, result.candidate_pool_size, rng);
        const auto pass_started_at = std::chrono::steady_clock::now();
        const auto window_bests = parallel_collect<RollingWindowBest>(
            windows.size(),
            result.worker_count,
            [&](std::size_t index) {
                return evaluate_window_candidates(windows[index], cfg, candidates);
            });
        const double pass_elapsed_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - pass_started_at).count();

        RollingPassSummary pass_summary;
        pass_summary.pass_index = pass_index;
        pass_summary.elapsed_seconds = pass_elapsed_seconds;
        pass_summary.evaluated_candidates_per_window = result.candidate_pool_size;
        pass_summary.median_config = median_config_from_window_bests(cfg, window_bests);
        pass_summary.window_bests = window_bests;

        std::vector<double> best_scores;
        std::vector<double> best_total_returns;
        std::vector<double> best_cagrs;
        std::vector<double> best_drawdowns;
        best_scores.reserve(window_bests.size());
        best_total_returns.reserve(window_bests.size());
        best_cagrs.reserve(window_bests.size());
        best_drawdowns.reserve(window_bests.size());
        for (const auto& best : window_bests) {
            best_scores.push_back(best.best_score);
            best_total_returns.push_back(best.best_mean_total_return_pct);
            best_cagrs.push_back(best.best_mean_cagr_pct);
            best_drawdowns.push_back(best.best_mean_max_drawdown_pct);
        }
        pass_summary.median_best_score = median_double(std::move(best_scores));
        pass_summary.median_best_mean_total_return_pct = median_double(std::move(best_total_returns));
        pass_summary.median_best_mean_cagr_pct = median_double(std::move(best_cagrs));
        pass_summary.median_best_mean_max_drawdown_pct = median_double(std::move(best_drawdowns));

        result.total_candidate_evaluations_completed += result.window_count * result.candidate_pool_size;
        result.pass_summaries.push_back(std::move(pass_summary));
        result.passes_completed = static_cast<int>(result.pass_summaries.size());
        current_center = result.pass_summaries.back().median_config;
        last_pass_elapsed_seconds = pass_elapsed_seconds;
    }

    if (result.pass_summaries.empty()) {
        throw std::runtime_error("No complete rolling-window pass completed.");
    }

    result.final_config = result.pass_summaries.back().median_config;
    result.replay_rows = parallel_collect<RollingReplayRow>(
        windows.size(),
        result.worker_count,
        [&](std::size_t index) {
            return replay_window(windows[index], result.final_config);
        });

    std::vector<double> replay_total_returns;
    std::vector<double> replay_cagrs;
    std::vector<double> replay_drawdowns;
    std::vector<double> replay_scores;
    replay_total_returns.reserve(result.replay_rows.size());
    replay_cagrs.reserve(result.replay_rows.size());
    replay_drawdowns.reserve(result.replay_rows.size());
    replay_scores.reserve(result.replay_rows.size());
    for (const auto& replay : result.replay_rows) {
        replay_total_returns.push_back(replay.mean_total_return_pct);
        replay_cagrs.push_back(replay.mean_cagr_pct);
        replay_drawdowns.push_back(replay.mean_max_drawdown_pct);
        replay_scores.push_back(replay.cagr_over_drawdown_ratio);
    }

    result.final_mean_total_return_pct = median_double(std::move(replay_total_returns));
    result.final_mean_cagr_pct = median_double(std::move(replay_cagrs));
    result.final_mean_max_drawdown_pct = median_double(std::move(replay_drawdowns));
    result.final_cagr_over_drawdown_ratio = median_double(std::move(replay_scores));
    result.elapsed_seconds_total = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at).count();
    return result;
}

std::string rolling_optimization_result_to_json(const RollingOptimizationResult& result) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    os << "{\n";
    os << "  \"objective_name\": \"" << result.objective_name << "\",\n";
    os << "  \"input_mode\": \"" << result.input_mode << "\",\n";
    os << "  \"input_file_count\": " << result.input_file_count << ",\n";
    os << "  \"common_start\": \"" << result.common_start << "\",\n";
    os << "  \"common_end\": \"" << result.common_end << "\",\n";
    os << "  \"window_count\": " << result.window_count << ",\n";
    os << "  \"window_months\": " << result.window_months << ",\n";
    os << "  \"window_step_months\": " << result.window_step_months << ",\n";
    os << "  \"candidate_pool_size\": " << result.candidate_pool_size << ",\n";
    os << "  \"parallel_workers_used\": " << result.worker_count << ",\n";
    os << "  \"max_passes\": " << result.max_passes << ",\n";
    os << "  \"passes_completed\": " << result.passes_completed << ",\n";
    os << "  \"total_candidate_evaluations_completed\": " << result.total_candidate_evaluations_completed << ",\n";
    os << "  \"rolling_total_time_budget_seconds\": " << result.total_time_budget_seconds << ",\n";
    os << "  \"elapsed_seconds_total\": " << result.elapsed_seconds_total << ",\n";
    os << "  \"stopped_by_time_budget\": " << (result.stopped_by_time_budget ? "true" : "false") << ",\n";
    os << "  \"final_parameters\": " << params_to_json(result.final_config, 2) << ",\n";
    os << "  \"final_median_results\": {\n";
    os << "    \"mean_total_return_pct\": " << result.final_mean_total_return_pct << ",\n";
    os << "    \"mean_cagr_pct\": " << result.final_mean_cagr_pct << ",\n";
    os << "    \"mean_max_drawdown_pct\": " << result.final_mean_max_drawdown_pct << ",\n";
    os << "    \"cagr_over_drawdown_ratio\": " << result.final_cagr_over_drawdown_ratio << "\n";
    os << "  },\n";
    os << "  \"pass_summaries\": [\n";
    for (std::size_t i = 0; i < result.pass_summaries.size(); ++i) {
        const auto& pass = result.pass_summaries[i];
        os << "    {\n";
        os << "      \"pass\": " << pass.pass_index << ",\n";
        os << "      \"elapsed_seconds\": " << pass.elapsed_seconds << ",\n";
        os << "      \"evaluated_candidates_per_window\": " << pass.evaluated_candidates_per_window << ",\n";
        os << "      \"median_parameters\": " << params_to_json(pass.median_config, 6) << ",\n";
        os << "      \"median_best_score\": " << pass.median_best_score << ",\n";
        os << "      \"median_best_mean_total_return_pct\": " << pass.median_best_mean_total_return_pct << ",\n";
        os << "      \"median_best_mean_cagr_pct\": " << pass.median_best_mean_cagr_pct << ",\n";
        os << "      \"median_best_mean_max_drawdown_pct\": " << pass.median_best_mean_max_drawdown_pct << ",\n";
        os << "      \"window_bests\": [\n";
        for (std::size_t j = 0; j < pass.window_bests.size(); ++j) {
            const auto& best = pass.window_bests[j];
            os << "        {\n";
            os << "          \"window_label\": \"" << best.window_label << "\",\n";
            os << "          \"window_start\": \"" << best.window_start << "\",\n";
            os << "          \"window_end\": \"" << best.window_end << "\",\n";
            os << "          \"best_parameters\": {\n";
            os << "            \"long_term_target_atr\": " << best.long_term_target_atr << ",\n";
            os << "            \"daily_target_atr\": " << best.daily_target_atr << ",\n";
            os << "            \"aggressiveness\": " << best.aggressiveness << "\n";
            os << "          },\n";
            os << "          \"best_score\": " << best.best_score << ",\n";
            os << "          \"best_mean_total_return_pct\": " << best.best_mean_total_return_pct << ",\n";
            os << "          \"best_mean_cagr_pct\": " << best.best_mean_cagr_pct << ",\n";
            os << "          \"best_mean_max_drawdown_pct\": " << best.best_mean_max_drawdown_pct << "\n";
            os << "        }";
            if (j + 1 != pass.window_bests.size()) {
                os << ',';
            }
            os << "\n";
        }
        os << "      ]\n";
        os << "    }";
        if (i + 1 != result.pass_summaries.size()) {
            os << ',';
        }
        os << "\n";
    }
    os << "  ],\n";
    os << "  \"replay_rows\": [\n";
    for (std::size_t i = 0; i < result.replay_rows.size(); ++i) {
        const auto& replay = result.replay_rows[i];
        os << "    {\n";
        os << "      \"window_label\": \"" << replay.window_label << "\",\n";
        os << "      \"window_start\": \"" << replay.window_start << "\",\n";
        os << "      \"window_end\": \"" << replay.window_end << "\",\n";
        os << "      \"mean_total_return_pct\": " << replay.mean_total_return_pct << ",\n";
        os << "      \"mean_cagr_pct\": " << replay.mean_cagr_pct << ",\n";
        os << "      \"mean_max_drawdown_pct\": " << replay.mean_max_drawdown_pct << ",\n";
        os << "      \"cagr_over_drawdown_ratio\": " << replay.cagr_over_drawdown_ratio << "\n";
        os << "    }";
        if (i + 1 != result.replay_rows.size()) {
            os << ',';
        }
        os << "\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os.str();
}

void write_rolling_optimization_artifacts(const std::filesystem::path& out_dir, const RollingOptimizationResult& result) {
    write_text(out_dir / "rolling_optimization_results.json", rolling_optimization_result_to_json(result));
}

}  // namespace cpp_backtester
