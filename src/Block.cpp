#include "Block.h"

BlockRotation rotateBlock(BlockRotation r, int k) {
    if (r == 0 && k == -1) return 3;
    return (r + k) % 4;
}

long long Block_TO_LONG(int x, int y) {
    return static_cast<long long>(x) << 32 | (y & 0xFFFFFFFFL);
}

BlockType::BlockType(int id, const std::function<bool(int)> &func) {
    ASSERT("Function is nullptr", func != nullptr);
    ASSERT("ID must be >= 0", id >= 0);
    this->id = id;
    isActive = func;
}

BlockType::BlockType() = default;

glm::mat4 &Block::getMVP() {
    return mvp;
}

Block::Block(int x, int y, BlockType *type, BlockRotation rotation) {
    ASSERT("Type is nullptr", type != nullptr);
    ASSERT("Rotation invalid", rotation >= 0 && rotation <= 3);
    this->type = type;
    this->rotation = rotation;
    updateMvp(x << 5, y << 5);
}

Block::Block(const char *buffer, BlockType *types, long long *pos) {
    ASSERT("Buffer is nullptr", buffer != nullptr);
    ASSERT("Position is nullptr", pos != nullptr);
    ASSERT("Types is nullptr", types != nullptr);
    memcpy(pos, buffer, 8);
    int t = 0;
    memcpy(&t, buffer + 8, 1);
    if (t < 0 || t > 14) {
        t = 0;
        PLOGW << "Block from buffer in " << *pos << " at " << (void *) buffer << " has invalid type, save corrupted";
    }
    type = &types[t];
    ASSERT("Type is nullptr", type != nullptr);
    ASSERT("Type isActive function is nullptr", type->isActive != nullptr);
    memcpy(&rotation, buffer + 9, 1);
    memcpy(&active, buffer + 10, 1);
    updateMvp(Block_X(*pos) << 5, Block_Y(*pos) << 5);
}

void Block::write(char *buffer, long long pos) {
    ASSERT("Buffer is nullptr", buffer != nullptr);
    memcpy(buffer, &pos, 8);
    memcpy(buffer + 8, &type->id, 1);
    memcpy(buffer + 9, &rotation, 1);
    memcpy(buffer + 10, &active, 1);
}

void Block::updateMvp(int x, int y) {
    mvp = glm::translate(glm::mat4(1), glm::vec3(x, y, -0.2f));
    mvp *= blockTransformMatrices[rotation];
}
