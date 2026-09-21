#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

constexpr int INF = 1000000000;

extern bool g_debug;

// Packs two integers into one lookup key.
inline uint64_t pack(int a, int b) {
    return (uint64_t)((uint64_t)(uint32_t)a << 32) | (uint32_t)b;
}

// Encodes a coordinate vector as a lookup key.
inline std::string keyOf(const std::vector<int>& v) {
    std::string k;
    k.reserve(v.size() * 6);
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) k.push_back('#');
        k += std::to_string(v[i]);
    }
    return k;
}

struct MemUsage {
    size_t workingKB;
    size_t peakKB;
};

MemUsage getMemUsage();
