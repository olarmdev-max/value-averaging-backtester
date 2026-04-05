#include "cpp_backtester/config.hpp"

#include <regex>

#include "cpp_backtester/io.hpp"

namespace cpp_backtester {
namespace {

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

}  // namespace

Config load_config(const std::filesystem::path& path) {
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

}  // namespace cpp_backtester
