#include "is_mpl/solver.h"

#include "graph_types.h"

#include <sys/resource.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace is_mpl {

using detail::ExpandedNode;
using detail::GraphNode;
using detail::coordinate_key;
using detail::state_key;
using detail::successor_targets;

namespace {

constexpr int kInfinity = 1'000'000'000;
constexpr int kUnreachableLevel = -1'000'000'000;

class SymbolTable {
public:
    void build(const std::vector<std::string>& sequences) {
        index_.fill(-1);
        symbols_.clear();

        std::unordered_set<char> unique_symbols;
        for (const auto& sequence : sequences) {
            for (char symbol : sequence) {
                unique_symbols.insert(symbol);
            }
        }

        symbols_.assign(unique_symbols.begin(), unique_symbols.end());
        std::sort(symbols_.begin(), symbols_.end());
        for (int i = 0; i < static_cast<int>(symbols_.size()); ++i) {
            index_[static_cast<unsigned char>(symbols_[i])] = i;
        }
    }

    int index(char symbol) const {
        return index_[static_cast<unsigned char>(symbol)];
    }

    int size() const {
        return static_cast<int>(symbols_.size());
    }

    const std::vector<char>& symbols() const {
        return symbols_;
    }

private:
    std::array<int, 256> index_{};
    std::vector<char> symbols_;
};

using NextTable = std::vector<std::vector<int>>;

NextTable build_next_table(
    const std::string& sequence,
    const SymbolTable& symbol_table) {
    const int length = static_cast<int>(sequence.size());
    const int symbol_count = symbol_table.size();
    NextTable next(length + 1, std::vector<int>(symbol_count, kInfinity));
    std::vector<int> last(symbol_count, kInfinity);

    for (int position = length; position >= 0; --position) {
        if (position < length) {
            const int symbol_index = symbol_table.index(sequence[position]);
            if (symbol_index >= 0) {
                last[symbol_index] = position + 1;
            }
        }
        for (int symbol_index = 0; symbol_index < symbol_count; ++symbol_index) {
            next[position][symbol_index] = last[symbol_index];
        }
    }
    return next;
}

struct ProcessMemory {
    std::size_t resident_kb = 0;
    std::size_t peak_resident_kb = 0;
};

ProcessMemory read_process_memory() {
    ProcessMemory memory;
    std::ifstream input("/proc/self/status");
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream fields(line);
            std::string key;
            std::string unit;
            fields >> key >> memory.resident_kb >> unit;
            break;
        }
    }

    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        memory.peak_resident_kb = static_cast<std::size_t>(usage.ru_maxrss);
    }
    return memory;
}

template <typename Node>
void backtrack_all(
    const std::vector<Node>& graph,
    const std::vector<int>& end_nodes,
    int limit,
    std::vector<std::string>& output) {
    std::unordered_set<std::string> unique_results;
    std::string reverse_buffer;

    std::function<void(int)> visit = [&](int node_id) {
        if (static_cast<int>(output.size()) >= limit) {
            return;
        }
        if (graph[node_id].level == 0) {
            std::string result(reverse_buffer);
            std::reverse(result.begin(), result.end());
            if (unique_results.insert(result).second) {
                output.push_back(std::move(result));
            }
            return;
        }
        for (int predecessor : graph[node_id].predecessors) {
            if (graph[predecessor].level == graph[node_id].level - 1) {
                reverse_buffer.push_back(graph[node_id].symbol);
                visit(predecessor);
                reverse_buffer.pop_back();
            }
        }
    };

    for (int node_id : end_nodes) {
        if (static_cast<int>(output.size()) >= limit) {
            break;
        }
        visit(node_id);
    }
}

}  // namespace

class Solver::Impl {
public:
    void set_debug(bool enabled) {
        debug = enabled;
    }

    void set_initial_sequences(const std::vector<std::string>& input_sequences) {
        if (input_sequences.empty()) {
            throw std::invalid_argument("at least one initial sequence is required");
        }
        sequences = input_sequences;
        sequence_count = static_cast<int>(sequences.size());
        symbol_table.build(sequences);
        next_tables.clear();
        next_tables.reserve(sequence_count);
        for (const auto& sequence : sequences) {
            next_tables.push_back(build_next_table(sequence, symbol_table));
        }
        graph.clear();
        coordinate_to_node.clear();
        vertex_layers.clear();
        expanded_graph.clear();
        expanded_state_to_node.clear();
        matched_position_layers.clear();
        last_new_sequence.clear();
        initial_graph_built = false;
        expansion_ready = false;
    }

    StageMetrics build_initial_dag(
        bool include_backtrack,
        int max_results,
        std::vector<std::string>* out_results) {
        if (sequences.empty()) {
            throw std::logic_error("set_initial_sequences() must be called first");
        }

        const auto memory_before = read_process_memory();
        const auto started = std::chrono::steady_clock::now();

        graph.clear();
        coordinate_to_node.clear();
        vertex_layers.clear();

        int maximum_sequence_length = 0;
        for (const auto& sequence : sequences) {
            maximum_sequence_length =
                std::max(maximum_sequence_length, static_cast<int>(sequence.size()));
        }
        vertex_layers.resize(maximum_sequence_length + 1);

        GraphNode source;
        source.id = 0;
        source.symbol = 'O';
        source.coordinates = std::vector<int>(sequence_count, 0);
        source.level = 0;
        graph.push_back(std::move(source));
        coordinate_to_node[coordinate_key(graph[0].coordinates)] = 0;
        vertex_layers[0].push_back(0);

        // Each VHT layer is traversed exactly once. This is the corrected
        // implementation used by the optimized IS+MPL experiments.
        for (int layer_index = 0;
             layer_index < static_cast<int>(vertex_layers.size());
             ++layer_index) {
            if (debug) {
                std::cerr << "[DBG] Process VHT layer i=" << layer_index
                          << " size=" << vertex_layers[layer_index].size() << '\n';
            }
            for (int predecessor_id : vertex_layers[layer_index]) {
                if (debug) {
                    std::cerr << "[DBG]  node pid=" << predecessor_id
                              << " level=" << graph[predecessor_id].level << '\n';
                }
                for (char symbol : symbol_table.symbols()) {
                    std::vector<int> coordinates(sequence_count);
                    bool feasible = true;
                    const int symbol_index = symbol_table.index(symbol);

                    for (int sequence_index = 0;
                         sequence_index < sequence_count;
                         ++sequence_index) {
                        const int old_position =
                            graph[predecessor_id].coordinates[sequence_index];
                        if (symbol_index < 0 || old_position < 0 ||
                            old_position >= static_cast<int>(next_tables[sequence_index].size()) ||
                            symbol_index >= static_cast<int>(next_tables[sequence_index][old_position].size())) {
                            feasible = false;
                            break;
                        }
                        const int next_position =
                            next_tables[sequence_index][old_position][symbol_index];
                        if (next_position >= kInfinity) {
                            feasible = false;
                            break;
                        }
                        coordinates[sequence_index] = next_position;
                    }

                    if (!feasible) {
                        continue;
                    }

                    const std::string key = coordinate_key(coordinates);
                    int successor_id = 0;
                    const auto existing = coordinate_to_node.find(key);
                    if (existing == coordinate_to_node.end()) {
                        GraphNode successor;
                        successor.id = static_cast<int>(graph.size());
                        successor.symbol = symbol;
                        successor.coordinates = coordinates;
                        successor.level = graph[predecessor_id].level + 1;
                        successor_id = successor.id;
                        graph.push_back(std::move(successor));
                        coordinate_to_node[key] = successor_id;

                        const int minimum_coordinate =
                            *std::min_element(coordinates.begin(), coordinates.end());
                        if (minimum_coordinate >= static_cast<int>(vertex_layers.size())) {
                            vertex_layers.resize(minimum_coordinate + 1);
                        }
                        vertex_layers[minimum_coordinate].push_back(successor_id);
                        graph[successor_id].predecessors.push_back(predecessor_id);
                    } else {
                        successor_id = existing->second;
                        auto& predecessors = graph[successor_id].predecessors;
                        if (std::find(predecessors.begin(), predecessors.end(), predecessor_id) ==
                            predecessors.end()) {
                            predecessors.push_back(predecessor_id);
                        }
                        graph[successor_id].level = std::max(
                            graph[successor_id].level,
                            graph[predecessor_id].level + 1);
                    }

                    auto& targets =
                        successor_targets(graph[predecessor_id].successors, symbol);
                    if (std::find(targets.begin(), targets.end(), successor_id) == targets.end()) {
                        targets.push_back(successor_id);
                    }
                }
            }
        }

        const auto finished = std::chrono::steady_clock::now();
        const auto memory_after = read_process_memory();

        StageMetrics metrics;
        metrics.build_ms =
            std::chrono::duration<double, std::milli>(finished - started).count();
        metrics.peak_kb = memory_after.peak_resident_kb;
        metrics.delta_kb = memory_after.resident_kb > memory_before.resident_kb
                               ? memory_after.resident_kb - memory_before.resident_kb
                               : 0;

        int maximum_level = 0;
        for (const auto& node : graph) {
            maximum_level = std::max(maximum_level, node.level);
        }
        metrics.mlcs_len = maximum_level;
        metrics.nodes_count = graph.size();

        // Later incremental steps reuse only graph edges. Coordinates, their
        // hash index, and successor tables for old sequences are no longer
        // queried, so release them after the initial DAG is complete.
        coordinate_to_node.clear();
        coordinate_to_node.rehash(0);
        next_tables.clear();
        next_tables.shrink_to_fit();
        for (auto& node : graph) {
            node.coordinates.clear();
            node.coordinates.shrink_to_fit();
        }

        if (include_backtrack) {
            const auto backtrack_started = std::chrono::steady_clock::now();
            std::vector<int> end_nodes;
            for (int node_id = 0; node_id < static_cast<int>(graph.size()); ++node_id) {
                if (graph[node_id].level == maximum_level) {
                    end_nodes.push_back(node_id);
                }
            }
            std::vector<std::string> results;
            backtrack_all(graph, end_nodes, max_results, results);
            const auto backtrack_finished = std::chrono::steady_clock::now();
            metrics.backtrack_ms = std::chrono::duration<double, std::milli>(
                                       backtrack_finished - backtrack_started)
                                       .count();
            if (out_results != nullptr) {
                *out_results = results;
            }
            metrics.results_count = results.size();
        }
        metrics.total_ms = metrics.build_ms + metrics.backtrack_ms;
        initial_graph_built = true;
        expansion_ready = false;
        return metrics;
    }

    StageMetrics expand_with_new_sequence(
        const std::string& new_sequence,
        bool include_backtrack,
        int max_results,
        std::vector<std::string>* out_results) {
        if (!initial_graph_built) {
            throw std::logic_error("build_initial_dag() must be called first");
        }
        if (expansion_ready) {
            throw std::logic_error(
                "finalize_expansion() must be called before adding another sequence");
        }

        const auto memory_before = read_process_memory();
        const auto started = std::chrono::steady_clock::now();

        const auto next_new = build_next_table(new_sequence, symbol_table);
        expanded_graph.clear();
        expanded_state_to_node.clear();
        matched_position_layers.clear();

        expanded_graph.reserve(graph.size());
        expanded_state_to_node.max_load_factor(0.80F);
        expanded_state_to_node.reserve(graph.size());

        const int source_id = ensure_expanded_node(0, 0).first;
        const int new_length = static_cast<int>(new_sequence.size());
        matched_position_layers.resize(new_length + 1);
        matched_position_layers[0].push_back(source_id);

        int tracked_maximum_level = 0;
        for (int position = 0; position <= new_length; ++position) {
            auto& layer = matched_position_layers[position];
            for (std::size_t item = 0; item < layer.size(); ++item) {
                const int predecessor_id = layer[item];
                const int old_node_id = expanded_graph[predecessor_id].original_id;
                const int predecessor_level = expanded_graph[predecessor_id].level;

                for (const auto& edge_group : graph[old_node_id].successors) {
                    const char symbol = edge_group.first;
                    const int symbol_index = symbol_table.index(symbol);
                    if (symbol_index < 0) {
                        continue;
                    }
                    const int next_position = next_new[position][symbol_index];
                    if (next_position >= kInfinity) {
                        continue;
                    }

                    for (int old_successor_id : edge_group.second) {
                        const auto state =
                            ensure_expanded_node(old_successor_id, next_position);
                        const int successor_id = state.first;
                        const int new_level = predecessor_level + 1;
                        auto& predecessors =
                            expanded_graph[successor_id].predecessors;

                        if (new_level > expanded_graph[successor_id].level) {
                            expanded_graph[successor_id].level = new_level;
                            predecessors.clear();
                            predecessors.push_back(predecessor_id);
                            tracked_maximum_level =
                                std::max(tracked_maximum_level, new_level);
                        } else if (new_level == expanded_graph[successor_id].level) {
                            // Each old-DAG edge and expanded state transition
                            // is emitted once in increasing matched-position order.
                            predecessors.push_back(predecessor_id);
                        }

                        successor_targets(
                            expanded_graph[predecessor_id].successors, symbol)
                            .push_back(successor_id);

                        // next_new strictly advances the matched position, so
                        // a newly created state needs exactly one enqueue.
                        if (state.second) {
                            matched_position_layers[next_position].push_back(successor_id);
                        }
                    }
                }
            }
        }

        const auto finished = std::chrono::steady_clock::now();
        const auto memory_after = read_process_memory();

        StageMetrics metrics;
        metrics.build_ms =
            std::chrono::duration<double, std::milli>(finished - started).count();
        metrics.peak_kb = memory_after.peak_resident_kb;
        metrics.delta_kb = memory_after.resident_kb > memory_before.resident_kb
                               ? memory_after.resident_kb - memory_before.resident_kb
                               : 0;
        metrics.mlcs_len = tracked_maximum_level;
        metrics.nodes_count = expanded_graph.size();

        if (include_backtrack) {
            const auto backtrack_started = std::chrono::steady_clock::now();
            std::vector<int> end_nodes;
            for (int node_id = 0;
                 node_id < static_cast<int>(expanded_graph.size());
                 ++node_id) {
                if (expanded_graph[node_id].level == tracked_maximum_level) {
                    end_nodes.push_back(node_id);
                }
            }
            std::vector<std::string> results;
            backtrack_all(expanded_graph, end_nodes, max_results, results);
            const auto backtrack_finished = std::chrono::steady_clock::now();
            metrics.backtrack_ms = std::chrono::duration<double, std::milli>(
                                       backtrack_finished - backtrack_started)
                                       .count();
            if (out_results != nullptr) {
                *out_results = results;
            }
            metrics.results_count = results.size();
        }

        metrics.total_ms = metrics.build_ms + metrics.backtrack_ms;
        last_new_sequence = new_sequence;
        expansion_ready = true;
        return metrics;
    }

    void finalize_expansion() {
        if (!expansion_ready) {
            throw std::logic_error(
                "expand_with_new_sequence() must be called before finalize_expansion()");
        }

        std::vector<GraphNode> next_graph;
        next_graph.reserve(expanded_graph.size());
        for (auto& expanded_node : expanded_graph) {
            GraphNode graph_node;
            graph_node.id = static_cast<int>(next_graph.size());
            graph_node.symbol = expanded_node.symbol;
            graph_node.level = expanded_node.level;
            graph_node.predecessors = std::move(expanded_node.predecessors);
            graph_node.successors = std::move(expanded_node.successors);
            next_graph.push_back(std::move(graph_node));
        }

        graph.swap(next_graph);
        expanded_graph.clear();
        expanded_graph.shrink_to_fit();
        expanded_state_to_node.clear();
        expanded_state_to_node.rehash(0);
        matched_position_layers.clear();
        matched_position_layers.shrink_to_fit();

        sequences.push_back(last_new_sequence);
        sequence_count = static_cast<int>(sequences.size());
        symbol_table.build(sequences);
        vertex_layers.clear();
        vertex_layers.shrink_to_fit();
        expansion_ready = false;
    }

private:
    std::pair<int, bool> ensure_expanded_node(int old_node_id, int new_position) {
        const std::uint64_t key = state_key(old_node_id, new_position);
        const int candidate_id = static_cast<int>(expanded_graph.size());
        const auto inserted = expanded_state_to_node.try_emplace(key, candidate_id);
        if (!inserted.second) {
            return {inserted.first->second, false};
        }

        ExpandedNode node;
        node.id = candidate_id;
        node.original_id = old_node_id;
        node.new_position = new_position;
        node.symbol = graph[old_node_id].symbol;
        node.level = old_node_id == 0 ? 0 : kUnreachableLevel;
        expanded_graph.push_back(std::move(node));
        return {candidate_id, true};
    }

    bool debug = false;
    bool initial_graph_built = false;
    bool expansion_ready = false;
    std::vector<std::string> sequences;
    int sequence_count = 0;
    SymbolTable symbol_table;
    std::vector<NextTable> next_tables;
    std::vector<GraphNode> graph;
    std::unordered_map<std::string, int> coordinate_to_node;
    std::vector<std::vector<int>> vertex_layers;

    std::vector<ExpandedNode> expanded_graph;
    std::unordered_map<std::uint64_t, int> expanded_state_to_node;
    std::vector<std::vector<int>> matched_position_layers;
    std::string last_new_sequence;
};

Solver::Solver() : impl_(std::make_unique<Impl>()) {}
Solver::~Solver() = default;
Solver::Solver(Solver&&) noexcept = default;
Solver& Solver::operator=(Solver&&) noexcept = default;

void Solver::set_debug(bool enabled) {
    impl_->set_debug(enabled);
}

void Solver::set_initial_sequences(const std::vector<std::string>& sequences) {
    impl_->set_initial_sequences(sequences);
}

StageMetrics Solver::build_initial_dag(
    bool include_backtrack,
    int max_results,
    std::vector<std::string>* out_results) {
    return impl_->build_initial_dag(include_backtrack, max_results, out_results);
}

StageMetrics Solver::expand_with_new_sequence(
    const std::string& sequence,
    bool include_backtrack,
    int max_results,
    std::vector<std::string>* out_results) {
    return impl_->expand_with_new_sequence(
        sequence, include_backtrack, max_results, out_results);
}

void Solver::finalize_expansion() {
    impl_->finalize_expansion();
}

}  // namespace is_mpl
