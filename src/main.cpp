#include <filesystem>
#include <iostream>
#include <string>

#include "cpp_backtester/config.hpp"
#include "cpp_backtester/io.hpp"
#include "cpp_backtester/monte_carlo.hpp"
#include "cpp_backtester/optimizer.hpp"
#include "cpp_backtester/price_generator.hpp"
#include "cpp_backtester/simulator.hpp"

namespace fs = std::filesystem;

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

        const auto cfg = cpp_backtester::load_config(config_path);

        if (command == "generate") {
            const auto bars = cpp_backtester::generate_prices(cfg);
            cpp_backtester::write_prices_csv(out_dir, bars);
            return 0;
        }

        if (command == "simulate") {
            const auto bars = cpp_backtester::generate_prices(cfg);
            cpp_backtester::write_prices_csv(out_dir, bars);
            const auto artifacts = cpp_backtester::run_simulation(cfg, bars);
            cpp_backtester::write_simulation_artifacts(out_dir, artifacts);
            cpp_backtester::write_text(out_dir / "summary.json", cpp_backtester::summary_to_json(cfg, artifacts.summary));
            return 0;
        }

        if (command == "monte-carlo") {
            const auto mc = cpp_backtester::run_monte_carlo(cfg);
            cpp_backtester::write_text(out_dir / "aggregate.json", cpp_backtester::monte_carlo_to_json(cfg, mc));
            return 0;
        }

        if (command == "optimize") {
            const auto result = cpp_backtester::optimize_config(cfg);
            cpp_backtester::write_optimization_artifacts(out_dir, result);
            return 0;
        }

        std::cerr << "Unknown command: " << command << '\n';
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
