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
