#include "Simulation.h"

#include <cstdlib>

namespace {
bool pointsAt(const Block &block, int sx, int sy, int tx, int ty) {
    auto points = [&](BlockRotation rotation, int length = 1) {
        switch (rotation) {
            case 0:
                return sx == tx && sy + length == ty;
            case 1:
                return sx + length == tx && sy == ty;
            case 2:
                return sx == tx && sy - length == ty;
            case 3:
                return sx - length == tx && sy == ty;
            default:
                return false;
        }
    };

    const BlockRotation r = block.rotation;
    switch (block.typeId) {
        case 0:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case BLOCK_CLOCK:
            return points(r);
        case 5:
            return points(r, 2);
        case 6:
            return points(r, 3);
        case 1:
            return points(r) || points(rotateBlock(r, 1));
        case 2:
            return points(r) || points(rotateBlock(r, -1));
        case 3:
            return points(rotateBlock(r, -1)) || points(rotateBlock(r, 1));
        case 4:
            return points(rotateBlock(r, -1)) || points(r) || points(rotateBlock(r, 1));
        case BLOCK_SWITCH:
        case BLOCK_BUTTON:
            return (std::abs(sx - tx) + std::abs(sy - ty)) == 1;
        default:
            return false;
    }
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

void Circuit::adjustConnection(int x, int y, BlockRotation rotation, int l, int delta,
                               std::unordered_set<long long> &affected) {
    switch (rotation) {
        case 0:
            adjustConnection(x, y + l, delta, affected);
            break;
        case 1:
            adjustConnection(x + l, y, delta, affected);
            break;
        case 2:
            adjustConnection(x, y - l, delta, affected);
            break;
        case 3:
            adjustConnection(x - l, y, delta, affected);
            break;
        default:
            break;
    }
}

void Circuit::adjustOutput(const Block &block, long long key, int delta,
                           std::unordered_set<long long> &affected) {
    int x = Block_X(key);
    int y = Block_Y(key);
    BlockRotation r = block.rotation;

    switch (block.typeId) {
        case 0: // Straight wire
        case 7: // NOT
        case 8: // AND
        case 9: // NAND
        case 10: // XOR
        case 11: // NXOR
        case BLOCK_CLOCK:
            adjustConnection(x, y, r, 1, delta, affected);
            break;
        case 5: // 2 wire
            adjustConnection(x, y, r, 2, delta, affected);
            break;
        case 6: // 3 wire
            adjustConnection(x, y, r, 3, delta, affected);
            break;
        case 1: // Right angled wire
            adjustConnection(x, y, r, 1, delta, affected);
            adjustConnection(x, y, rotateBlock(r, 1), 1, delta, affected);
            break;
        case 2: // Left angled wire
            adjustConnection(x, y, r, 1, delta, affected);
            adjustConnection(x, y, rotateBlock(r, -1), 1, delta, affected);
            break;
        case 3: // T wire
            adjustConnection(x, y, rotateBlock(r, -1), 1, delta, affected);
            adjustConnection(x, y, rotateBlock(r, 1), 1, delta, affected);
            break;
        case 4: // Cross wire
            adjustConnection(x, y, rotateBlock(r, -1), 1, delta, affected);
            adjustConnection(x, y, r, 1, delta, affected);
            adjustConnection(x, y, rotateBlock(r, 1), 1, delta, affected);
            break;
        case BLOCK_SWITCH:
        case BLOCK_BUTTON:
            adjustConnection(x + 1, y, delta, affected);
            adjustConnection(x, y - 1, delta, affected);
            adjustConnection(x, y + 1, delta, affected);
            adjustConnection(x - 1, y, delta, affected);
            break;
        default: // lamp emits nothing
            break;
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
