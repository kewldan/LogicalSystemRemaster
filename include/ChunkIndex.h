#pragma once

#include "Simulation.h"

#include <unordered_set>

// Spatial index over 32x32-block chunks, so drawing only walks chunks that
// overlap the view instead of the whole map.
class ChunkIndex {
public:
    static long long chunkOf(long long key) {
        return Block_TO_LONG(Block_X(key) >> 5, Block_Y(key) >> 5);
    }

    void insert(long long key) {
        chunks[chunkOf(key)].insert(key);
    }

    void erase(long long key) {
        auto it = chunks.find(chunkOf(key));
        if (it == chunks.end()) return;
        it->second.erase(key);
        if (it->second.empty()) chunks.erase(it);
    }

    void rebuild(const Blocks &blocks) {
        chunks.clear();
        for (auto &entry: blocks) insert(entry.first);
    }

    // Visits every block key inside chunks overlapping [x0..x1]x[y0..y1]
    // (inclusive block coordinates).
    template<typename F>
    void visit(int x0, int y0, int x1, int y1, F &&f) const {
        for (int cx = x0 >> 5; cx <= x1 >> 5; cx++) {
            for (int cy = y0 >> 5; cy <= y1 >> 5; cy++) {
                auto it = chunks.find(Block_TO_LONG(cx, cy));
                if (it == chunks.end()) continue;
                for (long long key: it->second) f(key);
            }
        }
    }

    [[nodiscard]] size_t chunkCount() const {
        return chunks.size();
    }

private:
    std::unordered_map<long long, std::unordered_set<long long>> chunks;
};
