#pragma once

#include "is_mpl/metrics.h"

#include <memory>
#include <string>
#include <vector>

namespace is_mpl {

// Incremental MLCS solver using immediate-successor reuse and
// matched-position layering (IS+MPL).
class Solver {
public:
    Solver();
    ~Solver();

    Solver(Solver&&) noexcept;
    Solver& operator=(Solver&&) noexcept;

    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;

    void set_debug(bool enabled);
    void set_initial_sequences(const std::vector<std::string>& sequences);

    StageMetrics build_initial_dag(
        bool include_backtrack = false,
        int max_results = 0,
        std::vector<std::string>* out_results = nullptr);

    StageMetrics expand_with_new_sequence(
        const std::string& sequence,
        bool include_backtrack = false,
        int max_results = 0,
        std::vector<std::string>* out_results = nullptr);

    // Promote the graph created by expand_with_new_sequence() so that the
    // next sequence can be added incrementally.
    void finalize_expansion();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace is_mpl
