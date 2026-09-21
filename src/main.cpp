#include "dmlcs.h"
#include "io_utils.h"
#include "common.h"
#include "metrics.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "  --input FILE          Initial sequences, one per line\n"
              << "  --add FILE            Sequences to add, one per line\n"
              << "  --measure MODE        MODE is build, include-backtrack, or full\n"
              << "  --max-results N       Return at most N MLCS results (default: 10)\n"
              << "  --no-finalize         Do not merge an expansion into the base graph\n"
              << "  --print-results       Print reconstructed MLCS results\n"
              << "  --debug               Enable diagnostic output\n"
              << "  --help                Show this help message\n";
}

std::string requireValue(int& index, int argc, char** argv) {
    if (index + 1 >= argc)
        throw std::invalid_argument(std::string("missing value for ") + argv[index]);
    return argv[++index];
}

int parseNonNegativeInt(const std::string& text, const std::string& option) {
    std::size_t parsed = 0;
    int value = 0;
    try {
        value = std::stoi(text, &parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid integer for " + option + ": " + text);
    }
    if (parsed != text.size() || value < 0)
        throw std::invalid_argument(option + " must be a non-negative integer");
    return value;
}
}

int main(int argc, char** argv) {
    try {
        std::string inputPath;
        std::string addPath;
        bool includeBacktrack = false;
        int maxResults = 10;
        bool finalizeEach = true;
        bool printResults = false;

        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--input") inputPath = requireValue(i, argc, argv);
            else if (argument == "--add") addPath = requireValue(i, argc, argv);
            else if (argument == "--measure") {
                const std::string mode = requireValue(i, argc, argv);
                if (mode != "build" && mode != "include-backtrack" && mode != "full")
                    throw std::invalid_argument("invalid --measure mode: " + mode);
                includeBacktrack = (mode != "build");
            } else if (argument == "--max-results") {
                maxResults = parseNonNegativeInt(requireValue(i, argc, argv), argument);
            } else if (argument == "--no-finalize") finalizeEach = false;
            else if (argument == "--print-results") printResults = true;
            else if (argument == "--debug") g_debug = true;
            else if (argument == "--help" || argument == "-h") {
                printUsage(argv[0]);
                return 0;
            } else {
                throw std::invalid_argument("unknown option: " + argument);
            }
        }

        std::vector<std::string> initialSequences;
        std::vector<std::string> addedSequences;
        if (!inputPath.empty()) initialSequences = readLines(inputPath);
        if (!addPath.empty()) addedSequences = readLines(addPath);
        if (inputPath.empty()) {
            initialSequences = {"ACGTACGT", "GACTAGTA"};
        } else if (initialSequences.empty()) {
            throw std::runtime_error("the initial-sequence file contains no sequences");
        }

        DMLCSN solver;
        solver.setInitialSequences(initialSequences);
        std::cout << "[Init] Initial sequences: " << initialSequences.size() << "\n"
                  << "[Init] Initial MLCS Length: " << solver.initialMLCSLength() << "\n"
                  << "[Init] Initial Nodes Count: " << solver.initialNodeCount() << "\n";

        for (std::size_t index = 0; index < addedSequences.size(); ++index) {
            std::cout << "[Run] Incremental build start (#" << (index + 1) << ")\n";
            std::vector<std::string> results;
            const StageMetrics metrics = solver.expandWithNewSequence(
                addedSequences[index], includeBacktrack, maxResults,
                includeBacktrack ? &results : nullptr);

            std::cout << "[Run] Incremental build end (#" << (index + 1) << ")\n"
                      << "After Add #" << (index + 1) << " MLCS Length: " << metrics.mlcs_len << "\n"
                      << "Nodes Count: " << metrics.nodes_count << "\n"
                      << "Incremental Build Time(s): " << (metrics.build_ms / 1000.0) << "\n"
                      << "Backtrack Time(s): " << (metrics.backtrack_ms / 1000.0) << "\n"
                      << "Total Time(s): " << (metrics.total_ms / 1000.0) << "\n"
                      << "Peak Memory(GB): " << (metrics.peak_kb / 1048576.0) << "\n"
                      << "Snapshot Delta(GB): " << (metrics.delta_kb / 1048576.0) << "\n";
            if (includeBacktrack) {
                std::cout << "Results Count: " << results.size() << "\n";
                if (printResults) {
                    for (const auto& result : results) std::cout << result << "\n";
                }
            }
            if (finalizeEach) solver.finalizeExpansionToGHT();
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\nUse --help for usage information.\n";
        return 1;
    }
}
