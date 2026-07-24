#include "Block.h"

#include <cassert>
#include <cstring>
#include "plog/Log.h"

BlockRotation rotateBlock(BlockRotation r, int k) {
    if (r == 0 && k == -1) return 3;
    return (r + k) % 4;
}

long long Block_TO_LONG(int x, int y) {
    return static_cast<long long>(x) << 32 | (y & 0xFFFFFFFFL);
}

bool isBlockActive(BlockId id, BlockConnectionCount connections) {
    switch (id) {
        case 7: // NOT
            return connections == 0;
        case 8: // AND
            return connections >= 2;
        case 9: // NAND
            return connections < 2;
        case 10: // XOR
            return connections % 2 != 0;
        case 11: // NXOR
            return connections % 2 == 0;
        case BLOCK_SWITCH:
        case BLOCK_BUTTON:
            return false;
        case BLOCK_CLOCK:
            return connections == 0;
        default: // wires & lamp
            return connections > 0;
    }
}

Block::Block(BlockId typeId, BlockRotation rotation) : typeId(typeId), rotation(rotation) {
    assert(typeId < BLOCK_TYPE_COUNT);
    assert(rotation <= 3);
}

Block::Block(const char *buffer, long long *pos) {
    assert(buffer != nullptr);
    assert(pos != nullptr);
    memcpy(pos, buffer, 8);
    typeId = static_cast<BlockId>(buffer[8]);
    if (typeId >= BLOCK_TYPE_COUNT) {
        PLOGW << "Block from buffer in " << *pos << " has invalid type " << (int) typeId << ", save corrupted";
        typeId = 0;
    }
    rotation = static_cast<BlockRotation>(buffer[9]) & 3;
    active = buffer[10] != 0;
}

void Block::write(char *buffer, long long pos) const {
    assert(buffer != nullptr);
    memcpy(buffer, &pos, 8);
    buffer[8] = static_cast<char>(typeId);
    buffer[9] = static_cast<char>(rotation);
    buffer[10] = active ? 1 : 0;
}
