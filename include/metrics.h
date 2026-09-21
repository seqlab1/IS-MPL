#pragma once
#include <cstddef>

struct StageMetrics {
    double build_ms = 0;
    double backtrack_ms = 0;
    double total_ms = 0;
    size_t peak_kb = 0;
    size_t delta_kb = 0;
    int mlcs_len = 0;
    size_t results_count = 0;
    size_t nodes_count = 0;
};
