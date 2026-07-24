#pragma once

#include "Block.h"

#include <unordered_map>
#include <unordered_set>

typedef std::unordered_map<long long, Block> Blocks;

// Event-driven simulation: a tick only walks the currently emitting blocks
// and re-evaluates cells whose inputs could have changed since the previous
// tick, so idle parts of a scheme cost nothing.
class Circuit {
public:
    Blocks blocks;

    void tick();

    // Re-sync bookkeeping for one cell after a local edit (place/erase/rotate/toggle)
    void invalidate(int x, int y);

    // Re-sync everything after a bulk edit (load/paste/clear/undo)
    void rebuild();

private:
    std::unordered_set<long long> emitters; // active blocks except clocks
    std::unordered_set<long long> clocks;
    std::unordered_set<long long> pending;  // cells to re-evaluate on the next tick
    std::unordered_map<long long, BlockConnectionCount> connections; // per-tick scratch

    void emit(const Block &block, long long key);

    void connect(int x, int y);

    void connect(int x, int y, BlockRotation rotation, int l);

    void evaluate(long long key, BlockConnectionCount count);
};
