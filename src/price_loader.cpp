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

bool looks_like_date_header(const std::string& token) {
    std::string lower;
    lower.reserve(token.size());
    for (unsigned char ch : token) {
        lower.push_back(static_cast<char>(std::tolower(ch)));
    }
    return lower == "date";
}

bool looks_like_iso_date(const std::string& token) {
    return token.size() == 10 &&
        std::isdigit(static_cast<unsigned char>(token[0])) &&
        std::isdigit(static_cast<unsigned char>(token[1])) &&
        std::isdigit(static_cast<unsigned char>(token[2])) &&
        std::isdigit(static_cast<unsigned char>(token[3])) &&
        token[4] == '-' &&
        std::isdigit(static_cast<unsigned char>(token[5])) &&
        std::isdigit(static_cast<unsigned char>(token[6])) &&
        token[7] == '-' &&
        std::isdigit(static_cast<unsigned char>(token[8])) &&
        std::isdigit(static_cast<unsigned char>(token[9]));
}

struct ParsedCloseSeries {
    std::vector<double> closes;
    std::vector<std::string> dates;
};

ParsedCloseSeries parse_close_values(const std::string& text, const std::string& path_string) {
    enum class CsvMode {
        kUnknown,
        kCloseOnly,
        kDateAndClose,
    };

    ParsedCloseSeries parsed;
    std::vector<double> closes;
    std::istringstream input(text);
    std::string line;
    bool first_non_empty_seen = false;
    CsvMode mode = CsvMode::kUnknown;

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

        std::string second_field;
        if (comma_pos != std::string::npos) {
            const auto second_comma_pos = line.find(',', comma_pos + 1);
            second_field = line.substr(comma_pos + 1, second_comma_pos == std::string::npos ? std::string::npos : second_comma_pos - comma_pos - 1);
            second_field = trim(second_field);
        }

        if (!first_non_empty_seen) {
            first_non_empty_seen = true;
            if (looks_like_header(first_field) && second_field.empty()) {
                mode = CsvMode::kCloseOnly;
                continue;
            }
            if (looks_like_date_header(first_field) && looks_like_header(second_field)) {
                mode = CsvMode::kDateAndClose;
                continue;
            }
        }

        if (mode == CsvMode::kUnknown) {
            mode = (!second_field.empty() && looks_like_iso_date(first_field))
                ? CsvMode::kDateAndClose
                : CsvMode::kCloseOnly;
        }

        try {
            if (mode == CsvMode::kDateAndClose) {
                if (!looks_like_iso_date(first_field)) {
                    throw std::runtime_error("Invalid ISO date in CSV: " + path_string + " -> '" + first_field + "'");
                }
                if (second_field.empty()) {
                    throw std::runtime_error("Missing close value in dated CSV: " + path_string + " -> '" + line + "'");
                }
                parsed.dates.push_back(first_field);
                closes.push_back(std::stod(second_field));
            } else {
                closes.push_back(std::stod(first_field));
            }
        } catch (const std::exception&) {
            if (mode == CsvMode::kDateAndClose) {
                throw std::runtime_error("Invalid dated CSV row: " + path_string + " -> '" + line + "'");
            }
            throw std::runtime_error("Invalid close value in CSV: " + path_string + " -> '" + first_field + "'");
        }
    }

    if (closes.empty()) {
        throw std::runtime_error("No close values found in CSV: " + path_string);
    }

    parsed.closes = std::move(closes);
    return parsed;
}

}  // namespace

PriceSeries load_close_series_csv(const std::filesystem::path& path, int atr_window) {
    const std::string text = read_file(path);
    const auto parsed = parse_close_values(text, path.string());

    PriceSeries series;
    series.name = path.stem().string();
    series.source_path = path.string();
    series.dates = parsed.dates;
    series.bars = build_bars_from_closes(parsed.closes, atr_window);
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
