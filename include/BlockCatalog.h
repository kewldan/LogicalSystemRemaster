#pragma once

#include "Block.h"

struct BlockDescription {
    const char *name;
    const char *rule;
};

struct BlockOutputs {
    int count;
    struct Cell {
        int dx, dy;
    } cells[4];
};

[[nodiscard]] const BlockDescription &describeBlock(BlockId id);

[[nodiscard]] BlockOutputs blockOutputs(BlockId id, BlockRotation rotation);
