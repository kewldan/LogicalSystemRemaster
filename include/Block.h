#pragma once

typedef unsigned char BlockRotation;
typedef unsigned char BlockId;
typedef unsigned char BlockConnectionCount;

constexpr BlockId BLOCK_TYPE_COUNT = 15;
constexpr BlockId BLOCK_SWITCH = 12;
constexpr BlockId BLOCK_CLOCK = 13;
constexpr int BLOCK_RECORD_SIZE = 11; // 8 bytes pos + type + rotation + active

BlockRotation rotateBlock(BlockRotation r, int k);

bool isBlockActive(BlockId id, BlockConnectionCount connections);

#define Block_X(l) static_cast<int>((l) >> 32)
#define Block_Y(l) static_cast<int>((l) & 0xFFFFFFFFL)

long long Block_TO_LONG(int x, int y);

class Block {
public:
    BlockId typeId{};
    BlockRotation rotation{};
    BlockConnectionCount connections{};
    bool active{}, selected{};

    Block() = default;

    Block(BlockId typeId, BlockRotation rotation);

    Block(const char *buffer, long long *pos);

    void write(char *buffer, long long pos) const;
};
