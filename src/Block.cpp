#include "Block.h"
#include "plog/Log.h"

BlockRotation rotateBlock(BlockRotation r, int k) {
    if (r == 0 && k == -1) return 3;
    return (r + k) % 4;
}

long long Block_TO_LONG(int x, int y) {
    return static_cast<long long>(x) << 32 | (y & 0xFFFFFFFFL);
}

BlockType::BlockType(BlockId id, const BlockActivationFunction &func) {
    assert(func != nullptr);
    assert(id >= 0);
    this->id = id;
    isActive = func;
}

BlockType::BlockType() = default;

glm::mat4 &Block::getMVP() {
    return mvp;
}

Block::Block(int x, int y, BlockType *type, BlockRotation rotation) {
    assert(type != nullptr);
    assert(rotation >= 0 && rotation <= 3);
    this->type = type;
    this->rotation = rotation;
    updateMvp(x, y);
}

Block::Block(const char *buffer, BlockType *types, long long *pos) {
    assert(buffer != nullptr);
    assert(pos != nullptr);
    assert(types != nullptr);
    memcpy(pos, buffer, 8);
    int t = 0;
    memcpy(&t, buffer + 8, 1);
    if (t < 0 || t > 14) {
        t = 0;
        PLOGW << "Block from buffer in " << *pos << " at " << (void *) buffer << " has invalid type, save corrupted";
    }
    type = &types[t];
    memcpy(&rotation, buffer + 9, 1);
    memcpy(&active, buffer + 10, 1);
    updateMvp(Block_X(*pos), Block_Y(*pos));
}

void Block::write(char *buffer, long long pos) {
    assert(buffer != nullptr);
    memcpy(buffer, &pos, 8);
    memcpy(buffer + 8, &type->id, 1);
    memcpy(buffer + 9, &rotation, 1);
    memcpy(buffer + 10, &active, 1);
}

void Block::updateMvp(int x, int y) {
    mvp = glm::translate(glm::mat4(1), glm::vec3(x << 5, y << 5, -0.2f));
    mvp *= blockTransformMatrices[rotation];
}
