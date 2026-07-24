#pragma once

#include "Block.h"

struct BlockDescription {
    const char *name;
    const char *rule;
};

[[nodiscard]] const BlockDescription &describeBlock(BlockId id);
