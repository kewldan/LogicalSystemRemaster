#include "BlockCatalog.h"

#include <array>

const BlockDescription &describeBlock(BlockId id) {
    static constexpr std::array<BlockDescription, BLOCK_TYPE_COUNT> descriptions{{
            {"Straight wire", "Passes a signal forward"},
            {"Right branch", "Passes a signal forward and right"},
            {"Left branch", "Passes a signal forward and left"},
            {"T branch", "Passes a signal left and right"},
            {"Cross branch", "Passes a signal forward, left and right"},
            {"Wire x2", "Passes a signal two cells forward"},
            {"Wire x3", "Passes a signal three cells forward"},
            {"NOT", "On when it has no active inputs"},
            {"AND", "On when at least two inputs are active"},
            {"NAND", "On when fewer than two inputs are active"},
            {"XOR", "On when an odd number of inputs is active"},
            {"XNOR", "On when an even number of inputs is active"},
            {"Switch", "Click to toggle a persistent signal"},
            {"Clock", "Toggles its signal every simulation tick"},
            {"Lamp", "Lights when it receives a signal"},
            {"Button", "Click to emit a one-tick pulse"}
    }};
    static constexpr BlockDescription unknown{"Unknown block", "No description available"};
    return id < descriptions.size() ? descriptions[id] : unknown;
}

namespace {
struct BlockShape {
    int count;
    struct {
        int dirDelta, length;
    } outputs[4];
};

constexpr std::array<BlockShape, BLOCK_TYPE_COUNT> shapes{{
        {1, {{0, 1}}},
        {2, {{0, 1}, {1, 1}}},
        {2, {{0, 1}, {-1, 1}}},
        {2, {{-1, 1}, {1, 1}}},
        {3, {{-1, 1}, {0, 1}, {1, 1}}},
        {1, {{0, 2}}},
        {1, {{0, 3}}},
        {1, {{0, 1}}},
        {1, {{0, 1}}},
        {1, {{0, 1}}},
        {1, {{0, 1}}},
        {1, {{0, 1}}},
        {4, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}},
        {1, {{0, 1}}},
        {0, {}},
        {4, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}}
}};

void directionOffset(BlockRotation direction, int length, int &dx, int &dy) {
    switch (direction) {
        case 0:
            dx = 0;
            dy = length;
            break;
        case 1:
            dx = length;
            dy = 0;
            break;
        case 2:
            dx = 0;
            dy = -length;
            break;
        default:
            dx = -length;
            dy = 0;
            break;
    }
}
}

BlockOutputs blockOutputs(BlockId id, BlockRotation rotation) {
    BlockOutputs result{};
    if (id >= shapes.size()) return result;
    const BlockShape &shape = shapes[id];
    for (int i = 0; i < shape.count; i++) {
        const BlockRotation direction = rotateBlock(rotation, shape.outputs[i].dirDelta);
        directionOffset(direction, shape.outputs[i].length,
                        result.cells[result.count].dx, result.cells[result.count].dy);
        result.count++;
    }
    return result;
}
