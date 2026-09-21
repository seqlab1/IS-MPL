#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>

#include "common.h"
#include "alphabet.h"
#include "graph_types.h"
#include "metrics.h"

class DMLCSN {
public:
    void setInitialSequences(const std::vector<std::string>& seqs);

    StageMetrics expandWithNewSequence(const std::string& newSeq,
                                       bool include_backtrack,
                                       int max_results,
                                       std::vector<std::string>* out_results);

    void finalizeExpansionToGHT();

    int initialMLCSLength() const { return initialMLCSLen_; }
    size_t initialNodeCount() const { return initialNodeCount_; }

private:
    void buildInitialGraph();

    std::vector<std::string> sequences;
    int m = 0;
    Alphabet alpha;
    std::vector<std::vector<std::vector<int>>> nextTables;

    std::vector<NodeG> GHT;
    std::unordered_map<std::string, int> key2G;
    std::vector<std::vector<int>> VHT;

    std::vector<NodeE> ENT;
    std::unordered_map<uint64_t, int> key2E;
    std::vector<std::vector<int>> INQ;
    std::vector<std::unordered_set<int>> INQset;
    std::string lastNewSeq;
    bool expansionPending = false;

    int initialMLCSLen_ = 0;
    size_t initialNodeCount_ = 0;

    int ensureEntNode(int originalId, int j);

    // Follows predecessors on longest paths only.
    template <typename NodeT>
    static void backtrackAll(const std::vector<NodeT>& graph,
                             const std::vector<int>& ends,
                             int limit,
                             std::vector<std::string>& out) {
        std::unordered_set<std::string> uniq;
        std::string buf;
        std::function<void(int)> dfs = [&](int v) {
            if ((int)out.size() >= limit) return;
            if (graph[v].level == 0) {
                std::string r(buf);
                std::reverse(r.begin(), r.end());
                if (!uniq.count(r)) {
                    uniq.insert(r);
                    out.push_back(r);
                }
                return;
            }
            for (int u : graph[v].preds) {
                if (graph[u].level == graph[v].level - 1) {
                    buf.push_back(graph[v].ch);
                    dfs(u);
                    buf.pop_back();
                }
            }
        };
        for (int v : ends) {
            if ((int)out.size() >= limit) break;
            dfs(v);
        }
    }
};
