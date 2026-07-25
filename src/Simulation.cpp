#include "Simulation.h"
#include "BlockCatalog.h"

namespace {
bool pointsAt(const Block &block, int sx, int sy, int tx, int ty) {
    const BlockOutputs outputs = blockOutputs(block.typeId, block.rotation);
    for (int i = 0; i < outputs.count; i++) {
        if (sx + outputs.cells[i].dx == tx && sy + outputs.cells[i].dy == ty) return true;
    }
    return false;
}
}

void Circuit::adjustConnection(int x, int y, int delta,
                               std::unordered_set<long long> &affected) {
    long long key = Block_TO_LONG(x, y);
    if (!blocks.contains(key)) return;

    int count = connections.contains(key) ? connections[key] : 0;
    count += delta;
    if (count <= 0) {
        connections.erase(key);
    } else {
        connections[key] = static_cast<BlockConnectionCount>(count);
    }
    affected.insert(key);
}

void Circuit::adjustOutput(const Block &block, long long key, int delta,
                           std::unordered_set<long long> &affected) {
    int x = Block_X(key);
    int y = Block_Y(key);
    const BlockOutputs outputs = blockOutputs(block.typeId, block.rotation);
    for (int i = 0; i < outputs.count; i++) {
        adjustConnection(x + outputs.cells[i].dx, y + outputs.cells[i].dy, delta, affected);
    }
}

void Circuit::evaluate(long long key, BlockConnectionCount count) {
    auto it = blocks.find(key);
    if (it == blocks.end()) return;
    Block &block = it->second;
    if (block.typeId == BLOCK_SWITCH || block.typeId == BLOCK_CLOCK || block.typeId == BLOCK_BUTTON) return;
    bool active = isBlockActive(block.typeId, count);
    if (active != block.active) {
        block.active = active;
        dirty.insert(key);
    }
}

void Circuit::tick() {
    for (auto it = clocks.begin(); it != clocks.end();) {
        auto found = blocks.find(*it);
        if (found == blocks.end()) {
            it = clocks.erase(it);
            continue;
        }
        found->second.active ^= 1;
        dirty.insert(*it);
        ++it;
    }

    // Work from a snapshot: outputs activated by evaluation below propagate
    // on the following tick, preserving the original one-cell-per-tick rule.
    std::unordered_set<long long> transitions = std::move(dirty);
    dirty.clear();
    std::unordered_set<long long> affected = std::move(pending);
    pending.clear();

    for (long long key: transitions) {
        auto old = applied.find(key);
        if (old != applied.end()) {
            adjustOutput(old->second, key, -1, affected);
            applied.erase(old);
        }

        auto current = blocks.find(key);
        if (current == blocks.end() || !current->second.active) continue;

        adjustOutput(current->second, key, 1, affected);
        applied[key] = current->second;
    }

    for (long long key: affected) {
        auto count = connections.find(key);
        evaluate(key, count == connections.end() ? 0 : count->second);
    }

    for (auto it = buttonPulses.begin(); it != buttonPulses.end();) {
        auto block = blocks.find(it->first);
        if (block == blocks.end() || block->second.typeId != BLOCK_BUTTON ||
            !block->second.active) {
            it = buttonPulses.erase(it);
            continue;
        }
        if (--it->second == 0) {
            block->second.active = false;
            dirty.insert(it->first);
            it = buttonPulses.erase(it);
        } else {
            ++it;
        }
    }
}

BlockConnectionCount Circuit::incomingCount(int x, int y) const {
    int count = 0;
    for (int distance = 1; distance <= 3; distance++) {
        const int candidates[4][2] = {
                {x - distance, y},
                {x + distance, y},
                {x, y - distance},
                {x, y + distance}
        };
        for (const auto &candidate: candidates) {
            auto it = applied.find(Block_TO_LONG(candidate[0], candidate[1]));
            if (it != applied.end() &&
                pointsAt(it->second, candidate[0], candidate[1], x, y)) {
                count++;
            }
        }
    }
    return static_cast<BlockConnectionCount>(count);
}

void Circuit::invalidate(int x, int y) {
    long long key = Block_TO_LONG(x, y);
    dirty.insert(key);
    auto it = blocks.find(key);
    if (it != blocks.end()) {
        const Block &block = it->second;
        pending.insert(key);
        BlockConnectionCount count = incomingCount(x, y);
        if (count == 0) connections.erase(key);
        else connections[key] = count;
        if (block.typeId == BLOCK_CLOCK) {
            clocks.insert(key);
            buttonPulses.erase(key);
        } else if (block.typeId == BLOCK_BUTTON && block.active) {
            clocks.erase(key);
            buttonPulses[key] = BUTTON_PULSE_TICKS;
        } else {
            clocks.erase(key);
            buttonPulses.erase(key);
        }
    } else {
        clocks.erase(key);
        buttonPulses.erase(key);
        pending.erase(key);
        connections.erase(key);
    }
}

void Circuit::rebuild() {
    clocks.clear();
    buttonPulses.clear();
    dirty.clear();
    pending.clear();
    connections.clear();
    applied.clear();
    for (auto &entry: blocks) {
        const Block &block = entry.second;
        pending.insert(entry.first);
        if (block.typeId == BLOCK_CLOCK) {
            clocks.insert(entry.first);
        } else if (block.active) {
            dirty.insert(entry.first);
            if (block.typeId == BLOCK_BUTTON) {
                buttonPulses[entry.first] = BUTTON_PULSE_TICKS;
            }
        }
    }
}
