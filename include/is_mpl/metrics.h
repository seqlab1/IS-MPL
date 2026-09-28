#pragma once

#include <cstddef>

namespace is_mpl {

struct StageMetrics {
    double build_ms = 0.0;
    double backtrack_ms = 0.0;
    double total_ms = 0.0;
    std::size_t peak_kb = 0;
    std::size_t delta_kb = 0;
    int mlcs_len = 0;
    std::size_t results_count = 0;
    std::size_t nodes_count = 0;
};

}  // namespace is_mpl
