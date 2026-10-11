#pragma once

#include <cstdint>
#include <vector>

#include "luckee/java_random.hpp"

namespace luckee {

// Recreates the six c0.0.13a_03 terrain passes: raising, eroding, soiling,
// carving, watering and melting. Dimensions use LuckeeMiner's convention:
// width = X, height = Z, depth = Y.
using ClassicGenerationProgress = void (*)(const char* stage, int percent);

std::vector<std::uint8_t> generateClassicWorld(
    int width, int height, int depth, JavaRandom& random,
    ClassicGenerationProgress progress = nullptr);

} // namespace luckee
