#include "luckee/level.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <cstdint>
#include <vector>

#include <zlib.h>

#include "luckee/perlin_noise.hpp"
#include "luckee/tile.hpp"

namespace luckee {

Level::Level(int width, int height, int depth)
    : width_(width),
      height_(height),
      depth_(depth),
      blocks_(
          static_cast<std::size_t>(width) *
          static_cast<std::size_t>(height) *
          static_cast<std::size_t>(depth),
          0),
      lightDepths_(
          static_cast<std::size_t>(width) *
          static_cast<std::size_t>(height),
          0),
      random_(std::random_device{}()) {
    initializeTiles();

    const bool mapLoaded = load();

    if (!mapLoaded)
        generateMap();

    calcLightDepths(
        0, 0,
        width_,
        height_);
}

std::size_t Level::index(
    int x, int y, int z) const {
    return (
        static_cast<std::size_t>(y) *
        static_cast<std::size_t>(height_) +
        static_cast<std::size_t>(z)) *
        static_cast<std::size_t>(width_) +
        static_cast<std::size_t>(x);
}

void Level::generateMap() {
    const std::vector<int> heightmap1 =
        PerlinNoiseFilter(0).read(
            width_, height_);

    const std::vector<int> heightmap2 =
        PerlinNoiseFilter(0).read(
            width_, height_);

    const std::vector<int> cf =
        PerlinNoiseFilter(1).read(
            width_, height_);

    const std::vector<int> rockMap =
        PerlinNoiseFilter(1).read(
            width_, height_);

    for (int x = 0; x < width_; ++x) {
        for (int y = 0; y < depth_; ++y) {
            for (int z = 0; z < height_; ++z) {
                const std::size_t column =
                    static_cast<std::size_t>(x) +
                    static_cast<std::size_t>(z) *
                        static_cast<std::size_t>(width_);

                int dh1 = heightmap1[column];
                int dh2 = heightmap2[column];

                if (cf[column] < 128)
                    dh2 = dh1;

                int dh = std::max(
                    dh1,
                    dh2);

                if (dh == dh2)
                    dh2 = dh1;

                dh =
                    dh / 8 +
                    depth_ / 3;

                int rh =
                    rockMap[column] / 8 +
                    depth_ / 3;

                rh = std::min(
                    rh,
                    dh - 2);

                int id = 0;

                if (y == dh)
                    id = Tile::grass->id();

                if (y < dh)
                    id = Tile::dirt->id();

                if (y <= rh)
                    id = Tile::rock->id();

                blocks_[index(x, y, z)] =
                    static_cast<std::uint8_t>(id);
            }
        }
    }
}

bool Level::load() {
    gzFile file =
        gzopen("level.dat", "rb");

    if (!file)
        return false;

    std::vector<std::uint8_t> loaded(
        blocks_.size());

    std::size_t offset = 0;

    while (offset < loaded.size()) {
        const unsigned int remaining =
            static_cast<unsigned int>(
                std::min<std::size_t>(
                    loaded.size() - offset,
                    0x7FFFFFFFu));

        const int read =
            gzread(
                file,
                loaded.data() + offset,
                remaining);

        if (read <= 0)
            break;

        offset +=
            static_cast<std::size_t>(read);
    }

    const int closeResult =
        gzclose(file);

    if (offset != loaded.size() ||
        closeResult != Z_OK) {
        return false;
    }

    blocks_.swap(loaded);
    calcLightDepths(
        0, 0,
        width_,
        height_);

    for (LevelListener* listener : listeners_) {
        if (listener)
            listener->allChanged();
    }

    return true;
}

void Level::save() const {
    gzFile file =
        gzopen("level.dat", "wb");

    if (!file) {
        std::perror("level.dat");
        return;
    }

    std::size_t offset = 0;

    while (offset < blocks_.size()) {
        const unsigned int remaining =
            static_cast<unsigned int>(
                std::min<std::size_t>(
                    blocks_.size() - offset,
                    0x7FFFFFFFu));

        const int written =
            gzwrite(
                file,
                blocks_.data() + offset,
                remaining);

        if (written <= 0)
            break;

        offset +=
            static_cast<std::size_t>(written);
    }

    gzclose(file);
}

void Level::calcLightDepths(
    int x0, int y0,
    int x1, int y1) {
    for (int x = x0;
         x < x0 + x1;
         ++x) {
        for (int z = y0;
             z < y0 + y1;
             ++z) {
            const int oldDepth =
                lightDepths_[
                    x + z * width_];

            int y = depth_ - 1;

            while (
                y > 0 &&
                !isLightBlocker(
                    x, y, z)) {
                --y;
            }

            lightDepths_[
                x + z * width_] = y;

            if (oldDepth == y)
                continue;

            const int yLow =
                std::min(oldDepth, y);
            const int yHigh =
                std::max(oldDepth, y);

            for (LevelListener* listener :
                 listeners_) {
                if (listener) {
                    listener->lightColumnChanged(
                        x, z,
                        yLow, yHigh);
                }
            }
        }
    }
}

void Level::addListener(
    LevelListener* listener) {
    if (!listener)
        return;

    if (std::find(
            listeners_.begin(),
            listeners_.end(),
            listener) == listeners_.end()) {
        listeners_.push_back(listener);
    }
}

void Level::removeListener(
    LevelListener* listener) {
    listeners_.erase(
        std::remove(
            listeners_.begin(),
            listeners_.end(),
            listener),
        listeners_.end());
}

int Level::getTile(
    int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ ||
        y >= depth_ ||
        z >= height_) {
        return 0;
    }

    return static_cast<int>(
        blocks_[index(x, y, z)]);
}

bool Level::isTile(
    int x, int y, int z) const {
    return getTile(x, y, z) != 0;
}

bool Level::isSolidTile(
    int x, int y, int z) const {
    const int id =
        getTile(x, y, z);

    Tile* tile =
        id >= 0 && id < Tile::MAX_TILES
            ? Tile::tiles[id]
            : nullptr;

    return tile &&
           tile->isSolid();
}

bool Level::isLightBlocker(
    int x, int y, int z) const {
    const int id =
        getTile(x, y, z);

    Tile* tile =
        id >= 0 && id < Tile::MAX_TILES
            ? Tile::tiles[id]
            : nullptr;

    return tile &&
           tile->blocksLight();
}

bool Level::isLit(
    int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ ||
        y >= depth_ ||
        z >= height_) {
        return true;
    }

    return y >=
        lightDepths_[
            x + z * width_];
}

void Level::getCubes(
    const AABB& box,
    std::vector<AABB>& result) const {
    result.clear();

    int x0 =
        static_cast<int>(box.x0);
    int x1 =
        static_cast<int>(
            box.x1 + 1.0f);
    int y0 =
        static_cast<int>(box.y0);
    int y1 =
        static_cast<int>(
            box.y1 + 1.0f);
    int z0 =
        static_cast<int>(box.z0);
    int z1 =
        static_cast<int>(
            box.z1 + 1.0f);

    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    z0 = std::max(z0, 0);
    x1 = std::min(x1, width_);
    y1 = std::min(y1, depth_);
    z1 = std::min(z1, height_);

    for (int x = x0; x < x1; ++x) {
        for (int y = y0; y < y1; ++y) {
            for (int z = z0; z < z1; ++z) {
                const int id =
                    getTile(x, y, z);

                Tile* tile =
                    id >= 0 &&
                    id < Tile::MAX_TILES
                        ? Tile::tiles[id]
                        : nullptr;

                if (!tile ||
                    !tile->isSolid()) {
                    continue;
                }

                result.push_back(
                    tile->getAABB(
                        x, y, z));
            }
        }
    }
}

float Level::getBrightness(
    int x, int y, int z) const {
    constexpr float dark = 0.8f;
    constexpr float light = 1.0f;

    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ ||
        y >= depth_ ||
        z >= height_) {
        return light;
    }

    return y <
        lightDepths_[
            x + z * width_]
        ? dark
        : light;
}

bool Level::setTile(
    int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ ||
        y >= depth_ ||
        z >= height_) {
        return false;
    }

    const std::uint8_t newType =
        static_cast<std::uint8_t>(
            type);

    if (blocks_[index(x, y, z)] ==
        newType) {
        return false;
    }

    blocks_[index(x, y, z)] =
        newType;

    calcLightDepths(
        x, z, 1, 1);

    for (LevelListener* listener :
         listeners_) {
        if (listener) {
            listener->tileChanged(
                x, y, z);
        }
    }

    return true;
}

void Level::tick() {
    // Match the reference's accumulated random-tile budget instead of
    // discarding fractional updates on each 20 TPS tick.
    unprocessed_ +=
        static_cast<std::uint64_t>(
            width_) *
        static_cast<std::uint64_t>(
            height_) *
        static_cast<std::uint64_t>(
            depth_);

    const std::uint64_t ticks =
        unprocessed_ /
        TILE_UPDATE_INTERVAL;

    unprocessed_ -=
        ticks *
        TILE_UPDATE_INTERVAL;

    for (std::uint64_t i = 0;
         i < ticks;
         ++i) {
        const int x =
            static_cast<int>(
                random_() %
                static_cast<std::uint32_t>(width_));
        const int y =
            static_cast<int>(
                random_() %
                static_cast<std::uint32_t>(depth_));
        const int z =
            static_cast<int>(
                random_() %
                static_cast<std::uint32_t>(height_));

        const int id =
            getTile(x, y, z);

        if (id < 0 ||
            id >= Tile::MAX_TILES) {
            continue;
        }

        Tile* tile =
            Tile::tiles[id];

        if (tile) {
            tile->tick(
                *this,
                x, y, z,
                random_);
        }
    }
}

} // namespace luckee
