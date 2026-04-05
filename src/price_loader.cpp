#include "cpp_backtester/price_loader.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpp_backtester/io.hpp"
#include "cpp_backtester/price_generator.hpp"

namespace cpp_backtester {
namespace {

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool looks_like_header(const std::string& token) {
    std::string lower;
    lower.reserve(token.size());
    for (unsigned char ch : token) {
        lower.push_back(static_cast<char>(std::tolower(ch)));
    }
    return lower == "close";
}

std::vector<double> parse_close_values(const std::string& text, const std::string& path_string) {
    std::vector<double> closes;
    std::istringstream input(text);
    std::string line;
    bool first_non_empty_seen = false;

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        std::string first_field = line;
        const auto comma_pos = line.find(',');
        if (comma_pos != std::string::npos) {
            first_field = line.substr(0, comma_pos);
        }
        first_field = trim(first_field);

        if (!first_non_empty_seen) {
            first_non_empty_seen = true;
            if (looks_like_header(first_field)) {
                continue;
            }
        }

        try {
            closes.push_back(std::stod(first_field));
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid close value in CSV: " + path_string + " -> '" + first_field + "'");
        }
    }

    if (closes.empty()) {
        throw std::runtime_error("No close values found in CSV: " + path_string);
    }

    return closes;
}

}  // namespace

PriceSeries load_close_series_csv(const std::filesystem::path& path, int atr_window) {
    const std::string text = read_file(path);
    const auto closes = parse_close_values(text, path.string());

    PriceSeries series;
    series.name = path.stem().string();
    series.source_path = path.string();
    series.bars = build_bars_from_closes(closes, atr_window);
    return series;
}

std::vector<PriceSeries> load_close_series_csvs(const std::vector<std::filesystem::path>& paths, int atr_window) {
    std::vector<PriceSeries> series_list;
    series_list.reserve(paths.size());
    for (const auto& path : paths) {
        series_list.push_back(load_close_series_csv(path, atr_window));
    }
    return series_list;
}

}  // namespace cpp_backtester
