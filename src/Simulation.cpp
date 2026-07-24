#include "Simulation.h"

void Circuit::connect(int x, int y) {
    long long key = Block_TO_LONG(x, y);
    if (blocks.contains(key)) {
        connections[key]++;
    }
}

void Circuit::connect(int x, int y, BlockRotation rotation, int l) {
    switch (rotation) {
        case 0:
            connect(x, y + l);
            break;
        case 1:
            connect(x + l, y);
            break;
        case 2:
            connect(x, y - l);
            break;
        case 3:
            connect(x - l, y);
            break;
        default:
            break;
    }
}

void Circuit::emit(const Block &block, long long key) {
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
            connect(x, y, r, 1);
            break;
        case 5: // 2 wire
            connect(x, y, r, 2);
            break;
        case 6: // 3 wire
            connect(x, y, r, 3);
            break;
        case 1: // Right angled wire
            connect(x, y, r, 1);
            connect(x, y, rotateBlock(r, 1), 1);
            break;
        case 2: // Left angled wire
            connect(x, y, r, 1);
            connect(x, y, rotateBlock(r, -1), 1);
            break;
        case 3: // T wire
            connect(x, y, rotateBlock(r, -1), 1);
            connect(x, y, rotateBlock(r, 1), 1);
            break;
        case 4: // Cross wire
            connect(x, y, rotateBlock(r, -1), 1);
            connect(x, y, r, 1);
            connect(x, y, rotateBlock(r, 1), 1);
            break;
        case BLOCK_SWITCH:
            connect(x + 1, y);
            connect(x, y - 1);
            connect(x, y + 1);
            connect(x - 1, y);
            break;
        default: // lamp emits nothing
            break;
    }
}

void Circuit::evaluate(long long key, BlockConnectionCount count) {
    auto it = blocks.find(key);
    if (it == blocks.end()) return;
    Block &block = it->second;
    if (block.typeId == BLOCK_SWITCH || block.typeId == BLOCK_CLOCK) return;
    bool active = isBlockActive(block.typeId, count);
    if (active != block.active) {
        block.active = active;
        if (active) {
            emitters.insert(key);
        } else {
            emitters.erase(key);
        }
    }
}

void Circuit::tick() {
    connections.clear();

    for (auto it = clocks.begin(); it != clocks.end();) {
        auto found = blocks.find(*it);
        if (found == blocks.end()) {
            it = clocks.erase(it);
            continue;
        }
        Block &clock = found->second;
        clock.active ^= 1;
        if (clock.active) emit(clock, *it);
        ++it;
    }

    for (auto it = emitters.begin(); it != emitters.end();) {
        auto found = blocks.find(*it);
        if (found == blocks.end() || !found->second.active) {
            it = emitters.erase(it);
            continue;
        }
        emit(found->second, *it);
        ++it;
    }

    // cells fed on the previous tick but not on this one drop back to zero inputs
    for (long long key: pending) {
        if (!connections.contains(key)) evaluate(key, 0);
    }
    pending.clear();
    for (auto &entry: connections) {
        evaluate(entry.first, entry.second);
        pending.insert(entry.first);
    }
}

void Circuit::invalidate(int x, int y) {
    long long key = Block_TO_LONG(x, y);
    auto it = blocks.find(key);
    if (it != blocks.end()) {
        const Block &block = it->second;
        pending.insert(key);
        if (block.typeId == BLOCK_CLOCK) {
            clocks.insert(key);
            emitters.erase(key);
        } else {
            clocks.erase(key);
            if (block.active) {
                emitters.insert(key);
            } else {
                emitters.erase(key);
            }
        }
    } else {
        emitters.erase(key);
        clocks.erase(key);
        pending.erase(key);
    }
}

void Circuit::rebuild() {
    emitters.clear();
    clocks.clear();
    pending.clear();
    connections.clear();
    for (auto &entry: blocks) {
        const Block &block = entry.second;
        pending.insert(entry.first);
        if (block.typeId == BLOCK_CLOCK) {
            clocks.insert(entry.first);
        } else if (block.active) {
            emitters.insert(entry.first);
        }
    }
}
