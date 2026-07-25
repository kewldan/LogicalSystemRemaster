#pragma once

#include "Block.h"

#include <ankerl/unordered_dense.h>
#include <unordered_map>

typedef std::unordered_map<long long, Block> Blocks;

using KeySet = ankerl::unordered_dense::set<long long>;

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
    KeySet clocks;
    ankerl::unordered_dense::map<long long, int> buttonPulses;
    KeySet dirty;    // outputs to apply on the next tick
    KeySet pending;  // cells to evaluate on the next tick
    ankerl::unordered_dense::map<long long, BlockConnectionCount> connections; // persistent input counts
    ankerl::unordered_dense::map<long long, Block> applied; // active output state currently reflected in connections

    void adjustOutput(const Block &block, long long key, int delta,
                      KeySet &affected);

    void adjustConnection(int x, int y, int delta,
                          KeySet &affected);

    void evaluate(long long key, BlockConnectionCount count);

    BlockConnectionCount incomingCount(int x, int y) const;
};
