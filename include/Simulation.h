#pragma once

#include "Block.h"

#include <unordered_map>
#include <unordered_set>

typedef std::unordered_map<long long, Block> Blocks;

// Event-driven simulation. Stable signals are kept as persistent connection
// counts; a tick only applies blocks whose output changed and re-evaluates
// their destinations. This matters for large sequential schemes, where half
// of every latch can remain active for millions of ticks without doing work.
class Circuit {
public:
    Blocks blocks;

    void tick();

    // Re-sync bookkeeping for one cell after a local edit (place/erase/rotate/toggle)
    void invalidate(int x, int y);

    // Re-sync everything after a bulk edit (load/paste/clear/undo)
    void rebuild();

private:
    std::unordered_set<long long> clocks;
    std::unordered_map<long long, int> buttonPulses;
    std::unordered_set<long long> dirty;    // outputs to apply on the next tick
    std::unordered_set<long long> pending;  // cells to evaluate on the next tick
    std::unordered_map<long long, BlockConnectionCount> connections; // persistent input counts
    Blocks applied; // active output state currently reflected in connections

    void adjustOutput(const Block &block, long long key, int delta,
                      std::unordered_set<long long> &affected);

    void adjustConnection(int x, int y, int delta,
                          std::unordered_set<long long> &affected);

    void evaluate(long long key, BlockConnectionCount count);

    BlockConnectionCount incomingCount(int x, int y) const;
};
