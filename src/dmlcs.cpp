#include "dmlcs.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

void DMLCSN::setInitialSequences(const std::vector<std::string>& seqs) {
    if (seqs.empty())
        throw std::invalid_argument("at least one initial sequence is required");

    sequences = seqs;
    expansionPending = false;
    lastNewSeq.clear();
    m = (int)sequences.size();
    alpha.build(sequences);

    nextTables.clear();
    nextTables.reserve(m);
    for (int i = 0; i < m; ++i)
        nextTables.push_back(buildNext(sequences[i], alpha));

    buildInitialGraph();
}

void DMLCSN::buildInitialGraph() {
    GHT.clear();
    key2G.clear();
    VHT.clear();

    int maxLen = 0;
    for (auto &s : sequences) maxLen = std::max(maxLen, (int)s.size());
    VHT.resize(maxLen + 1);

    // Create the source node.
    NodeG src;
    src.id = 0;
    src.ch = 'O';
    src.coord = std::vector<int>(m, 0);
    src.level = 0;
    GHT.push_back(src);
    key2G[keyOf(src.coord)] = 0;
    VHT[0].push_back(0);

    for (int li = 0; li < (int)VHT.size(); ++li) {
        if (g_debug)
            std::cerr << "[DBG] Process VHT layer i=" << li
                      << " size=" << VHT[li].size() << "\n";

        for (int pid : VHT[li]) {
            for (char c : alpha.list) {
                std::vector<int> coords(m);
                bool ok = true;
                int k = alpha.index(c);

                for (int t = 0; t < m; ++t) {
                    if (k < 0) { ok = false; break; }
                    int pc = GHT[pid].coord[t];
                    if (pc < 0 || pc >= (int)nextTables[t].size()) { ok = false; break; }
                    if (k >= (int)nextTables[t][pc].size()) { ok = false; break; }
                    int pos = nextTables[t][pc][k];
                    if (pos >= INF) { ok = false; break; }
                    coords[t] = pos;
                }
                if (!ok) continue;

                std::string key = keyOf(coords);
                int qid;
                auto itq = key2G.find(key);
                if (itq == key2G.end()) {
                    NodeG q;
                    q.id = (int)GHT.size();
                    q.ch = c;
                    q.coord = coords;
                    q.level = GHT[pid].level + 1;
                    GHT.push_back(q);
                    qid = q.id;
                    key2G[key] = qid;

                    int mn = *std::min_element(coords.begin(), coords.end());
                    if (mn > (int)VHT.size() - 1) VHT.resize(mn + 1);
                    VHT[mn].push_back(qid);
                    GHT[qid].preds.push_back(pid);
                } else {
                    qid = itq->second;
                    auto &preds = GHT[qid].preds;
                    if (std::find(preds.begin(), preds.end(), pid) == preds.end())
                        preds.push_back(pid);
                    if (GHT[qid].level < GHT[pid].level + 1)
                        GHT[qid].level = GHT[pid].level + 1;
                }

                auto &vec = GHT[pid].succ[c];
                if (std::find(vec.begin(), vec.end(), qid) == vec.end())
                    vec.push_back(qid);
            }
        }
    }

    int maxL = 0;
    for (auto &n : GHT) maxL = std::max(maxL, n.level);
    initialMLCSLen_ = maxL;
    initialNodeCount_ = GHT.size();
}

int DMLCSN::ensureEntNode(int originalId, int j) {
    uint64_t k = pack(originalId, j);
    auto it = key2E.find(k);
    if (it != key2E.end()) return it->second;

    NodeE e;
    e.id = (int)ENT.size();
    e.originalId = originalId;
    e.j = j;
    e.ch = GHT[originalId].ch;
    e.level = (originalId == 0 ? 0 : -1000000000);
    e.coord = GHT[originalId].coord;
    e.coord.push_back(j);
    ENT.push_back(e);
    key2E[k] = e.id;
    return e.id;
}

StageMetrics DMLCSN::expandWithNewSequence(const std::string& newSeq,
                                           bool include_backtrack,
                                           int max_results,
                                           std::vector<std::string>* out_results) {
    if (GHT.empty())
        throw std::logic_error("setInitialSequences must be called before expansion");
    if (max_results < 0)
        throw std::invalid_argument("max_results must not be negative");
    if (out_results) out_results->clear();
    expansionPending = false;

    auto mem0 = getMemUsage();
    auto t0 = std::chrono::steady_clock::now();

    auto nextNew = buildNext(newSeq, alpha);
    ENT.clear(); key2E.clear();
    INQ.clear(); INQset.clear();

    int srcEnt = ensureEntNode(0, 0);

    int L = (int)newSeq.size();
    INQ.resize(L + 1);
    INQset.resize(L + 1);
    INQ[0].push_back(srcEnt);
    INQset[0].insert(srcEnt);

    for (int j = 0; j <= L; ++j) {
        auto &layer = INQ[j];

        // Mark characters still available after position j.
        std::vector<char> present(alpha.size(), 0);
        for (int k = 0; k < alpha.size(); ++k)
            present[k] = nextNew[j][k] < INF ? 1 : 0;

        for (size_t xi = 0; xi < layer.size(); ++xi) {
            int pid = layer[xi];
            int originalId = ENT[pid].originalId;
            int peLevel = ENT[pid].level;

            for (auto &kv : GHT[originalId].succ) {
                char c = kv.first;
                int k = alpha.index(c);
                if (k < 0 || !present[k]) continue;

                int j2 = nextNew[j][k];
                if (j2 >= INF) continue;

                for (int qg : kv.second) {
                    int qEnt = ensureEntNode(qg, j2);
                    int newLevel = peLevel + 1;

                    auto &pv = ENT[qEnt].preds;
                    if (newLevel > ENT[qEnt].level) {
                        ENT[qEnt].level = newLevel;
                        pv.clear();
                        pv.push_back(pid);
                    } else if (newLevel == ENT[qEnt].level) {
                        if (std::find(pv.begin(), pv.end(), pid) == pv.end())
                            pv.push_back(pid);
                    }

                    auto &se = ENT[pid].succ[c];
                    if (std::find(se.begin(), se.end(), qEnt) == se.end())
                        se.push_back(qEnt);

                    if (!INQset[j2].count(qEnt)) {
                        INQset[j2].insert(qEnt);
                        INQ[j2].push_back(qEnt);
                    }
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    auto mem1 = getMemUsage();

    StageMetrics met;
    met.build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    met.peak_kb = mem1.peakKB;
    met.delta_kb = mem1.workingKB > mem0.workingKB ? (mem1.workingKB - mem0.workingKB) : 0;

    int maxL = 0;
    for (auto &n : ENT) maxL = std::max(maxL, n.level);
    met.mlcs_len = maxL;
    met.nodes_count = ENT.size();

    if (include_backtrack) {
        auto tb0 = std::chrono::steady_clock::now();
        std::vector<int> ends;
        for (int i = 0; i < (int)ENT.size(); ++i)
            if (ENT[i].level == maxL) ends.push_back(i);

        std::vector<std::string> res;
        backtrackAll(ENT, ends, max_results, res);

        auto tb1 = std::chrono::steady_clock::now();
        met.backtrack_ms = std::chrono::duration<double, std::milli>(tb1 - tb0).count();
        if (out_results) *out_results = res;
        met.results_count = res.size();
    }

    met.total_ms = met.build_ms + met.backtrack_ms;
    lastNewSeq = newSeq;
    expansionPending = true;
    return met;
}

void DMLCSN::finalizeExpansionToGHT() {
    if (!expansionPending)
        throw std::logic_error("no expansion is available to finalize");

    // Keep only reachable states.
    std::vector<int> keep;
    keep.reserve(ENT.size());
    for (int i = 0; i < (int)ENT.size(); ++i)
        if (ENT[i].level >= 0) keep.push_back(i);

    std::vector<int> oldToNew(ENT.size(), -1);
    for (int i = 0; i < (int)keep.size(); ++i)
        oldToNew[keep[i]] = i;

    std::vector<NodeG> NG;
    NG.reserve(keep.size());
    for (int oldId : keep) {
        NodeG g;
        g.id = (int)NG.size();
        g.ch = ENT[oldId].ch;
        g.coord = ENT[oldId].coord;
        g.level = ENT[oldId].level;
        NG.push_back(g);
    }

    std::unordered_map<std::string, int> mapNew;
    for (int i = 0; i < (int)keep.size(); ++i)
        mapNew[keyOf(NG[i].coord)] = i;

    for (int i = 0; i < (int)keep.size(); ++i) {
        int oldId = keep[i];
        for (auto &kv : ENT[oldId].succ) {
            char c = kv.first;
            auto &vec = NG[i].succ[c];
            for (int qe : kv.second) {
                int nq = oldToNew[qe];
                if (nq >= 0) vec.push_back(nq);
            }
        }
        for (int p : ENT[oldId].preds) {
            int np = oldToNew[p];
            if (np >= 0) NG[i].preds.push_back(np);
        }
    }

    GHT.swap(NG);
    key2G.swap(mapNew);
    sequences.push_back(lastNewSeq);
    m = (int)sequences.size();
    alpha.build(sequences);

    nextTables.clear();
    nextTables.reserve(m);
    for (int i = 0; i < m; ++i)
        nextTables.push_back(buildNext(sequences[i], alpha));

    VHT.clear();
    expansionPending = false;
}
