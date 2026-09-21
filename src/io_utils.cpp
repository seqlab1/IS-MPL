#include "io_utils.h"

#include <fstream>
#include <stdexcept>

std::vector<std::string> readLines(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open input file: " + path);
    std::vector<std::string> lines;
    std::string s;
    while (std::getline(in, s)) {
        if (!s.empty() && s.back() == '\r') s.pop_back();
        if (!s.empty()) lines.push_back(s);
    }
    if (in.bad()) throw std::runtime_error("failed while reading input file: " + path);
    return lines;
}
