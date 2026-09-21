#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct Alphabet {
    std::unordered_map<char, int> idx;
    std::vector<char> list;

    void build(const std::vector<std::string>& seqs);

    int index(char c) const;

    int size() const;
};

// Stores the next 1-based position of each character, or INF when absent.
std::vector<std::vector<int>> buildNext(const std::string& s, const Alphabet& alpha);
