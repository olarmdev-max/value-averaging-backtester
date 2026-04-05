#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "cpp_backtester/config.hpp"
#include "cpp_backtester/io.hpp"
#include "cpp_backtester/monte_carlo.hpp"
#include "cpp_backtester/optimizer.hpp"
#include "cpp_backtester/price_generator.hpp"
#include "cpp_backtester/price_loader.hpp"
#include "cpp_backtester/rolling_optimizer.hpp"
#include "cpp_backtester/simulator.hpp"

namespace fs = std::filesystem;

namespace {

std::vector<fs::path> collect_price_files(int argc, char** argv, int start_index) {
    std::vector<fs::path> paths;
    for (int i = start_index; i < argc; ++i) {
        paths.emplace_back(argv[i]);
    }
    return paths;
}

std::string sanitize_name(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char ch : name) {
        if (std::isalnum(ch) || ch == '-' || ch == '_') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('_');
        }
    }
    return out.empty() ? std::string("series") : out;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 4) {
            std::cerr << "Usage: cpp_backtester <generate|simulate|monte-carlo|optimize|optimize-rolling> <config.json> <output_dir> [price_file.csv ...]\n";
            return 1;
        }

        const std::string command = argv[1];
        const fs::path config_path = argv[2];
        const fs::path out_dir = argv[3];
        fs::create_directories(out_dir);

        const auto cfg = cpp_backtester::load_config(config_path);
        const auto price_file_paths = collect_price_files(argc, argv, 4);
        const auto input_series = price_file_paths.empty()
            ? std::vector<cpp_backtester::PriceSeries>{}
            : cpp_backtester::load_close_series_csvs(price_file_paths, cfg.atr_window);

        if (command == "generate") {
            if (!input_series.empty()) {
                std::cerr << "The generate command does not accept input price files.\n";
                return 1;
            }
            const auto bars = cpp_backtester::generate_prices(cfg);
            cpp_backtester::write_prices_csv(out_dir, bars);
            return 0;
        }

        if (command == "simulate") {
            if (input_series.empty()) {
                const auto bars = cpp_backtester::generate_prices(cfg);
                cpp_backtester::write_prices_csv(out_dir, bars);
                const auto artifacts = cpp_backtester::run_simulation(cfg, bars);
                cpp_backtester::write_simulation_artifacts(out_dir, artifacts);
                cpp_backtester::write_text(out_dir / "summary.json", cpp_backtester::summary_to_json(cfg, artifacts.summary));
                return 0;
            }

            std::vector<cpp_backtester::SimSummary> summaries;
            for (const auto& series : input_series) {
                const auto target_dir = (input_series.size() == 1) ? out_dir : (out_dir / sanitize_name(series.name));
                fs::create_directories(target_dir);
                cpp_backtester::write_prices_csv(target_dir, series.bars);
                const auto artifacts = cpp_backtester::run_simulation(cfg, series.bars);
                cpp_backtester::write_simulation_artifacts(target_dir, artifacts);
                cpp_backtester::write_text(target_dir / "summary.json", cpp_backtester::summary_to_json(cfg, artifacts.summary));
                summaries.push_back(artifacts.summary);
            }

            if (input_series.size() > 1) {
                const auto aggregate = cpp_backtester::run_price_series_batch(cfg, input_series);
                cpp_backtester::write_text(out_dir / "aggregate.json", cpp_backtester::monte_carlo_to_json(cfg, aggregate));
            }
            return 0;
        }

        if (command == "monte-carlo") {
            const auto mc = input_series.empty()
                ? cpp_backtester::run_monte_carlo(cfg)
                : cpp_backtester::run_price_series_batch(cfg, input_series);
            cpp_backtester::write_text(out_dir / "aggregate.json", cpp_backtester::monte_carlo_to_json(cfg, mc));
            return 0;
        }

        if (command == "optimize") {
            const auto result = cpp_backtester::optimize_config(cfg, input_series);
            cpp_backtester::write_optimization_artifacts(out_dir, result);
            return 0;
        }

        if (command == "optimize-rolling") {
            const auto result = cpp_backtester::optimize_rolling_windows(cfg, input_series);
            cpp_backtester::write_rolling_optimization_artifacts(out_dir, result);
            return 0;
        }

        std::cerr << "Unknown command: " << command << '\n';
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
