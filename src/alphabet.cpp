#include "alphabet.h"
#include "common.h"

#include <algorithm>
#include <unordered_set>

void Alphabet::build(const std::vector<std::string>& seqs) {
    idx.clear();
    list.clear();

    std::unordered_set<char> s;
    for (const auto& x : seqs)
        for (char c : x)
            s.insert(c);

    for (char c : s) list.push_back(c);
    std::sort(list.begin(), list.end());

    idx.clear();
    for (int i = 0; i < (int)list.size(); ++i)
        idx[list[i]] = i;
}

int Alphabet::index(char c) const {
    auto it = idx.find(c);
    return it == idx.end() ? -1 : it->second;
}

int Alphabet::size() const {
    return (int)list.size();
}

std::vector<std::vector<int>> buildNext(const std::string& s, const Alphabet& alpha) {
    int L = (int)s.size();
    int K = alpha.size();

    std::vector<std::vector<int>> nxt(L + 1, std::vector<int>(K, INF));
    std::vector<int> last(K, INF);

    for (int j = L; j >= 0; --j) {
        if (j < L) {
            int k = alpha.index(s[j]);
            if (k >= 0) last[k] = j + 1;
        }
        for (int k = 0; k < K; ++k)
            nxt[j][k] = last[k];
    }
    return nxt;
}
