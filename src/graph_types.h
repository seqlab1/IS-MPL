#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace is_mpl::detail {

using Successors = std::vector<std::pair<char, std::vector<int>>>;

inline std::vector<int>& successor_targets(Successors& successors, char symbol) {
    for (auto& entry : successors) {
        if (entry.first == symbol) {
            return entry.second;
        }
    }
    successors.emplace_back(symbol, std::vector<int>{});
    return successors.back().second;
}

struct GraphNode {
    int id = 0;
    char symbol = '\0';
    std::vector<int> coordinates;
    int level = 0;
    std::vector<int> predecessors;
    Successors successors;
};

struct ExpandedNode {
    int id = 0;
    int original_id = 0;
    int new_position = 0;
    char symbol = '\0';
    int level = 0;
    std::vector<int> predecessors;
    Successors successors;
};

inline std::string coordinate_key(const std::vector<int>& coordinates) {
    std::string key;
    key.reserve(coordinates.size() * 6);
    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        if (i != 0) {
            key.push_back('#');
        }
        key += std::to_string(coordinates[i]);
    }
    return key;
}

inline std::uint64_t state_key(int old_node_id, int new_position) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(old_node_id)) << 32U) |
           static_cast<std::uint32_t>(new_position);
}

}  // namespace is_mpl::detail
