#include "cpp_backtester/io.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cpp_backtester {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void write_text(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write file: " + path.string());
    }
    out << content;
}

}  // namespace cpp_backtester
