#include "luckee/classic_worldgen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "luckee/classic_noise.hpp"

namespace luckee {
namespace {

constexpr std::uint8_t ROCK = 1;
constexpr std::uint8_t GRASS = 2;
constexpr std::uint8_t DIRT = 3;
constexpr std::uint8_t STILL_WATER = 9;
constexpr std::uint8_t STILL_LAVA = 11;

std::size_t blockIndex(int x, int y, int z, int width, int height) {
    return (static_cast<std::size_t>(y) *
            static_cast<std::size_t>(height) +
            static_cast<std::size_t>(z)) *
           static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x);
}

bool inside(
    int x, int y, int z,
    int width, int height, int depth) {
    return x >= 0 && x < width &&
           y >= 0 && y < depth &&
           z >= 0 && z < height;
}

// Classic's flood fill expands along an X run and explores only sideways or
// downward. This keeps the frontier much smaller than a per-block 3D queue.
std::uint64_t floodFill(
    std::vector<std::uint8_t>& blocks,
    int width, int height, int depth,
    int startX, int startY, int startZ,
    std::uint8_t fluidId) {
    if (!inside(startX, startY, startZ, width, height, depth))
        return 0;

    std::vector<std::uint32_t> frontier;
    frontier.reserve(1024);
    frontier.push_back(static_cast<std::uint32_t>(
        blockIndex(startX, startY, startZ, width, height)));

    std::uint64_t filled = 0;

    const auto read = [&](int x, int y, int z) -> std::uint8_t {
        if (!inside(x, y, z, width, height, depth))
            return 0xFF;
        return blocks[blockIndex(x, y, z, width, height)];
    };

    while (!frontier.empty()) {
        const std::uint32_t packed = frontier.back();
        frontier.pop_back();

        const int x = static_cast<int>(packed % static_cast<std::uint32_t>(width));
        const std::uint32_t column = packed / static_cast<std::uint32_t>(width);
        const int z = static_cast<int>(column % static_cast<std::uint32_t>(height));
        const int y = static_cast<int>(column / static_cast<std::uint32_t>(height));

        if (!inside(x, y, z, width, height, depth) ||
            read(x, y, z) != 0) {
            continue;
        }

        int left = x;
        int right = x;
        while (left > 0 && read(left - 1, y, z) == 0)
            --left;
        while (right + 1 < width && read(right + 1, y, z) == 0)
            ++right;

        for (int fillX = left; fillX <= right; ++fillX) {
            blocks[blockIndex(fillX, y, z, width, height)] = fluidId;
            ++filled;
        }

        bool previousDown = false;
        bool previousNegZ = false;
        bool previousPosZ = false;

        for (int fillX = left; fillX <= right; ++fillX) {
            const int nextY = y - 1;
            const int nextNegZ = z - 1;
            const int nextPosZ = z + 1;

            auto queueNeighbor = [&](int nx, int ny, int nz,
                                     bool& previousEmpty, bool below) {
                std::uint8_t neighbor = read(nx, ny, nz);
                // Classic's lava pass turns water directly beneath lava into
                // rock; runtime neighbor callbacks implement the other cases.
                if (fluidId == STILL_LAVA && below &&
                    (neighbor == 8 || neighbor == 9) &&
                    inside(nx, ny, nz, width, height, depth)) {
                    blocks[blockIndex(nx, ny, nz, width, height)] = ROCK;
                    neighbor = ROCK;
                }
                const bool empty = neighbor == 0;
                if (empty && !previousEmpty && inside(nx, ny, nz, width, height, depth)) {
                    frontier.push_back(static_cast<std::uint32_t>(
                        blockIndex(nx, ny, nz, width, height)));
                }
                previousEmpty = empty;
            };

            queueNeighbor(fillX, nextY, z, previousDown, true);
            queueNeighbor(fillX, y, nextNegZ, previousNegZ, false);
            queueNeighbor(fillX, y, nextPosZ, previousPosZ, false);
        }
    }

    return filled;
}

} // namespace

std::vector<std::uint8_t> generateClassicWorld(
    int width, int height, int depth, JavaRandom& random,
    ClassicGenerationProgress progress) {
    const auto report = [&](const char* stage, int percent) {
        if (progress)
            progress(stage, std::max(0, std::min(100, percent)));
    };
    const std::size_t planeSize =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t totalSize =
        planeSize * static_cast<std::size_t>(depth);

    std::vector<std::uint8_t> blocks(totalSize, 0);
    std::vector<int> heights(planeSize, 0);

    // Construct fields in explicit order because their permutations consume
    // one shared Java Random stream in the original generator.
    ClassicOctaveNoise raiseA1(random, 8);
    ClassicOctaveNoise raiseA2(random, 8);
    ClassicDistortedNoise raisingA(raiseA1, raiseA2);
    ClassicOctaveNoise raiseB1(random, 8);
    ClassicOctaveNoise raiseB2(random, 8);
    ClassicDistortedNoise raisingB(raiseB1, raiseB2);
    ClassicOctaveNoise selector(random, 8);

    // Raising.
    report("Raising terrain", 0);
    for (int x = 0; x < width; ++x) {
        if ((x & 7) == 0)
            report("Raising terrain", x * 100 / width);
        for (int z = 0; z < height; ++z) {
            const double first = raisingA.sample(x, z) / 8.0 - 8.0;
            double second = raisingB.sample(x, z) / 8.0 + 8.0;
            if (selector.sample(x, z) / 8.0 > 2.0)
                second = first;

            double value = std::max(first, second);
            value = ((value * value * value / 100.0) + value * 3.0) / 8.0;
            heights[static_cast<std::size_t>(x) +
                    static_cast<std::size_t>(z) * static_cast<std::size_t>(width)] =
                static_cast<int>(value);
        }
    }

    report("Raising terrain", 100);
    // Eroding.
    report("Eroding terrain", 0);
    ClassicOctaveNoise erosionA1(random, 8);
    ClassicOctaveNoise erosionA2(random, 8);
    ClassicDistortedNoise erosionNoise(erosionA1, erosionA2);
    ClassicOctaveNoise erosionB1(random, 8);
    ClassicOctaveNoise erosionB2(random, 8);
    ClassicDistortedNoise erosionSelector(erosionB1, erosionB2);

    for (int x = 0; x < width; ++x) {
        if ((x & 7) == 0)
            report("Eroding terrain", x * 100 / width);
        for (int z = 0; z < height; ++z) {
            const double noise = erosionNoise.sample(x << 1, z << 1) / 8.0;
            const int selectorBit =
                erosionSelector.sample(x << 1, z << 1) > 0.0 ? 1 : 0;
            if (noise > 2.0) {
                int& h = heights[static_cast<std::size_t>(x) +
                                 static_cast<std::size_t>(z) *
                                     static_cast<std::size_t>(width)];
                h = (((h - selectorBit) / 2) * 2) + selectorBit;
            }
        }
    }

    report("Eroding terrain", 100);
    // Soiling. Classic uses Y = heightmap + depth/2, with two dirt layers.
    report("Soiling terrain", 0);
    for (int x = 0; x < width; ++x) {
        if ((x & 7) == 0)
            report("Soiling terrain", x * 100 / width);
        for (int y = 0; y < depth; ++y) {
            for (int z = 0; z < height; ++z) {
                const int top =
                    heights[static_cast<std::size_t>(x) +
                            static_cast<std::size_t>(z) *
                                static_cast<std::size_t>(width)] +
                    depth / 2;
                const int dirtLine = top - 2;
                std::uint8_t id = 0;

                if (y == top && y >= depth / 2 - 1)
                    id = GRASS;
                else if (y <= top)
                    id = DIRT;

                if (y <= dirtLine)
                    id = ROCK;

                blocks[blockIndex(x, y, z, width, height)] = id;
            }
        }
    }

    report("Soiling terrain", 100);
    // Carving. This follows the original ellipsoidal worm walk and its
    // asymmetrical vertical weighting.
    report("Carving caves", 0);
    const int caveCount = width * height * depth / 256 / 64;
    for (int cave = 0; cave < caveCount; ++cave) {
        if ((cave & 7) == 0)
            report("Carving caves", cave * 100 / std::max(1, caveCount));
        float x = random.nextFloat() * width;
        float y = random.nextFloat() * depth;
        float z = random.nextFloat() * height;
        const int length = static_cast<int>(
            random.nextFloat() + random.nextFloat() * 150.0f);
        float yaw = random.nextFloat() *
            6.28318530717958647692f;
        float pitch = random.nextFloat() *
            6.28318530717958647692f;
        float yawDrift = 0.0f;
        float pitchDrift = 0.0f;

        for (int step = 0; step < length; ++step) {
            x += std::sin(yaw) * std::cos(pitch);
            z += std::cos(yaw) * std::cos(pitch);
            y += std::sin(pitch);
            yaw += yawDrift * 0.2f;
            yawDrift = yawDrift * 0.9f +
                (random.nextFloat() - random.nextFloat());
            pitch = (pitch + pitchDrift * 0.5f) * 0.5f;
            pitchDrift = pitchDrift * 0.9f +
                (random.nextFloat() - random.nextFloat());

            const float radius =
                static_cast<float>(
                    std::sin(step * 3.14159265358979323846 / length) * 2.5 + 1.0);
            const int x0 = static_cast<int>(x - radius);
            const int x1 = static_cast<int>(x + radius);
            const int y0 = static_cast<int>(y - radius);
            const int y1 = static_cast<int>(y + radius);
            const int z0 = static_cast<int>(z - radius);
            const int z1 = static_cast<int>(z + radius);

            for (int xx = x0; xx <= x1; ++xx) {
                for (int yy = y0; yy <= y1; ++yy) {
                    for (int zz = z0; zz <= z1; ++zz) {
                        const float dx = xx - x;
                        const float dy = yy - y;
                        const float dz = zz - z;
                        if (dx * dx + dy * dy * 2.0f + dz * dz >=
                                radius * radius ||
                            xx < 1 || yy < 1 || zz < 1 ||
                            xx >= width - 1 || yy >= depth - 1 ||
                            zz >= height - 1) {
                            continue;
                        }

                        auto& block = blocks[blockIndex(xx, yy, zz, width, height)];
                        if (block == ROCK)
                            block = 0;
                    }
                }
            }
        }
    }

    report("Carving caves", 100);
    // Watering from the perimeter at sea level and from random interior
    // openings. The fill uses STILL_WATER (9), as the original did.
    report("Filling water", 0);
    const int waterY = depth / 2 - 1;
    for (int x = 0; x < width; ++x) {
        if ((x & 7) == 0)
            report("Filling water", x * 45 / width);
        floodFill(blocks, width, height, depth, x, waterY, 0, STILL_WATER);
        floodFill(blocks, width, height, depth, x, waterY, height - 1, STILL_WATER);
    }
    for (int z = 0; z < height; ++z) {
        floodFill(blocks, width, height, depth, 0, waterY, z, STILL_WATER);
        floodFill(blocks, width, height, depth, width - 1, waterY, z, STILL_WATER);
    }

    report("Filling water", 55);
    const int waterSources = width * height / 200;
    for (int source = 0; source < waterSources; ++source) {
        if ((source & 15) == 0)
            report("Filling water",
                55 + source * 45 / std::max(1, waterSources));
        const int x = random.nextInt(width);
        const int y = depth / 2 - 1 - random.nextInt(3);
        const int z = random.nextInt(height);
        if (blocks[blockIndex(x, y, z, width, height)] == 0)
            floodFill(blocks, width, height, depth, x, y, z, STILL_WATER);
    }

    report("Filling water", 100);
    // Melting. The source count and vertical range match c0.0.13a_03.
    report("Generating lava", 0);
    const int lavaSources = static_cast<int>(
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) *
        static_cast<std::size_t>(depth) / 10000u);
    for (int source = 0; source < lavaSources; ++source) {
        if ((source & 15) == 0)
            report("Generating lava", source * 100 / std::max(1, lavaSources));
        const int x = random.nextInt(width);
        const int y = random.nextInt(std::max(1, depth / 2 - 4));
        const int z = random.nextInt(height);
        if (blocks[blockIndex(x, y, z, width, height)] == 0)
            floodFill(blocks, width, height, depth, x, y, z, STILL_LAVA);
    }

    report("Generating lava", 100);
    return blocks;
}

} // namespace luckee
