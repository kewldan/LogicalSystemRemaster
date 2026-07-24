#pragma once

#include "Simulation.h"

#include <cstdint>
#include <vector>

struct SchemeCamera {
    float x{}, y{};
    float zoom{1.f};
};

// Parses a scheme from a BSON (.bson) or legacy binary (.ls) buffer.
// On failure returns false and leaves the outputs untouched.
bool schemeFromMemory(const char *data, int length, bool isBson, Blocks &outBlocks, SchemeCamera &outCamera);

std::vector<std::uint8_t> schemeToBson(const Blocks &blocks, const SchemeCamera &camera);
