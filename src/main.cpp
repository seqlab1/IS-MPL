#include "is_mpl/solver.h"

#include "io.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string mode = "auto";
    std::string input_path;
    std::string additions_path;
    bool include_backtrack = false;
    bool finalize_each = true;
    bool print_results = false;
    bool debug = false;
    int max_results = 10;
};

void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Options:\n"
        << "  --input FILE              Initial sequences, one per line\n"
        << "  --add FILE                Sequences to add, one per line\n"
        << "  --mode auto|initial       Auto selects incremental mode when --add is nonempty\n"
        << "  --measure build-only      Exclude MLCS backtracking (default)\n"
        << "  --measure include-backtrack\n"
        << "                            Include MLCS backtracking after each update\n"
        << "  --max-results N           Maximum strings returned by backtracking\n"
        << "  --print-results           Print returned MLCS strings\n"
        << "  --no-finalize             Do not promote the expanded graph (one addition only)\n"
        << "  --debug                   Print initial-DAG traversal diagnostics\n"
        << "  --help                    Show this message\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        auto require_value = [&](const std::string& name) -> std::string {
            if (index + 1 >= argc) {
                throw std::invalid_argument("missing value for " + name);
            }
            return argv[++index];
        };

        if (argument == "--input") {
            options.input_path = require_value(argument);
        } else if (argument == "--add") {
            options.additions_path = require_value(argument);
        } else if (argument == "--mode") {
            options.mode = require_value(argument);
            if (options.mode != "auto" && options.mode != "initial") {
                throw std::invalid_argument("--mode must be auto or initial");
            }
        } else if (argument == "--measure") {
            const std::string value = require_value(argument);
            if (value == "include-backtrack" || value == "full") {
                options.include_backtrack = true;
            } else if (value == "build-only") {
                options.include_backtrack = false;
            } else {
                throw std::invalid_argument(
                    "--measure must be build-only or include-backtrack");
            }
        } else if (argument == "--max-results") {
            options.max_results = std::stoi(require_value(argument));
            if (options.max_results < 0) {
                throw std::invalid_argument("--max-results must be nonnegative");
            }
        } else if (argument == "--no-finalize") {
            options.finalize_each = false;
        } else if (argument == "--print-results") {
            options.print_results = true;
        } else if (argument == "--debug") {
            options.debug = true;
        } else if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }
    return options;
}

void print_results(const std::vector<std::string>& results, bool print_strings) {
    std::cout << "Results Count: " << results.size() << '\n';
    if (print_strings) {
        for (const auto& result : results) {
            std::cout << result << '\n';
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        std::vector<std::string> initial_sequences;
        std::vector<std::string> additions;
        if (!options.input_path.empty()) {
            initial_sequences =
                is_mpl::detail::read_nonempty_lines(options.input_path);
        }
        if (!options.additions_path.empty()) {
            additions =
                is_mpl::detail::read_nonempty_lines(options.additions_path);
        }
        if (initial_sequences.empty()) {
            initial_sequences = {"ACGTACGT", "GACTAGTA"};
        }
        if (!options.finalize_each && additions.size() > 1) {
            throw std::invalid_argument(
                "--no-finalize supports at most one added sequence");
        }

        is_mpl::Solver solver;
        solver.set_debug(options.debug);
        solver.set_initial_sequences(initial_sequences);

        if (options.mode == "initial" || additions.empty()) {
            std::cout << "[Run] Static build start\n";
            std::vector<std::string> results;
            const is_mpl::StageMetrics metrics = solver.build_initial_dag(
                options.include_backtrack,
                options.max_results,
                options.include_backtrack ? &results : nullptr);
            std::cout << "Static Build MLCS Length: " << metrics.mlcs_len << '\n';
            std::cout << "Nodes Count: " << metrics.nodes_count << '\n';
            std::cout << "Build Time(s): " << metrics.build_ms / 1000.0 << '\n';
            std::cout << "Backtrack Time(s): " << metrics.backtrack_ms / 1000.0 << '\n';
            std::cout << "Total Time(s): " << metrics.total_ms / 1000.0 << '\n';
            std::cout << "Peak Memory(GB): " << metrics.peak_kb / 1048576.0 << '\n';
            std::cout << "Snapshot Delta(GB): " << metrics.delta_kb / 1048576.0 << '\n';
            if (options.include_backtrack) {
                print_results(results, options.print_results);
            }
            std::cout << "[Run] Static build end\n";
            return 0;
        }

        std::cout << "[Run] Initial static build start\n";
        const is_mpl::StageMetrics initial_metrics =
            solver.build_initial_dag(false, 0, nullptr);
        std::cout << "[Run] Initial static build end\n";
        std::cout << "Initial Static Build MLCS Length: "
                  << initial_metrics.mlcs_len << '\n';
        std::cout << "Nodes Count: " << initial_metrics.nodes_count << '\n';
        std::cout << "Initial Build Time(s): "
                  << initial_metrics.build_ms / 1000.0 << '\n';
        std::cout << "Initial Peak Memory(GB): "
                  << initial_metrics.peak_kb / 1048576.0 << '\n';
        std::cout << "Initial Snapshot Delta(GB): "
                  << initial_metrics.delta_kb / 1048576.0 << '\n';

        for (std::size_t index = 0; index < additions.size(); ++index) {
            std::cout << "[Run] Incremental build start (#" << index + 1 << ")\n";
            std::vector<std::string> results;
            const is_mpl::StageMetrics metrics = solver.expand_with_new_sequence(
                additions[index],
                options.include_backtrack,
                options.max_results,
                options.include_backtrack ? &results : nullptr);
            std::cout << "[Run] Incremental build end (#" << index + 1 << ")\n";
            std::cout << "After Add #" << index + 1
                      << " MLCS Length: " << metrics.mlcs_len << '\n';
            std::cout << "Nodes Count: " << metrics.nodes_count << '\n';
            std::cout << "Incremental Build Time(s): "
                      << metrics.build_ms / 1000.0 << '\n';
            std::cout << "Backtrack Time(s): "
                      << metrics.backtrack_ms / 1000.0 << '\n';
            std::cout << "Total Time(s): " << metrics.total_ms / 1000.0 << '\n';
            std::cout << "Peak Memory(GB): "
                      << metrics.peak_kb / 1048576.0 << '\n';
            std::cout << "Snapshot Delta(GB): "
                      << metrics.delta_kb / 1048576.0 << '\n';
            if (options.include_backtrack) {
                print_results(results, options.print_results);
            }
            if (options.finalize_each) {
                solver.finalize_expansion();
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
