#pragma once
#include <vector>
#include <unordered_map>

struct NodeG {
    int id;
    char ch;
    std::vector<int> coord;
    int level;
    std::vector<int> preds;
    std::unordered_map<char, std::vector<int>> succ;
};

struct NodeE {
    int id;
    int originalId;
    int j;
    char ch;
    std::vector<int> coord;
    int level;
    std::vector<int> preds;
    std::unordered_map<char, std::vector<int>> succ;
};
