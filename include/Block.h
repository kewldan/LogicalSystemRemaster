#pragma once

typedef unsigned char BlockRotation;
typedef unsigned char BlockId;
typedef unsigned char BlockConnectionCount;

constexpr BlockId BLOCK_TYPE_COUNT = 16;
constexpr BlockId BLOCK_SWITCH = 12;
constexpr BlockId BLOCK_CLOCK = 13;
constexpr BlockId BLOCK_LAMP = 14;
constexpr BlockId BLOCK_BUTTON = 15; // emits a single-tick pulse when clicked
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
    bool active{}, selected{};

    Block() = default;

    Block(BlockId typeId, BlockRotation rotation);

    Block(const char *buffer, long long *pos);

    void write(char *buffer, long long pos) const;
};
