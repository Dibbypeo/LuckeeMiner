#include "luckee/level.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

#include <zlib.h>

namespace luckee {

Level::Level(int width, int height, int depth)
    : width_(width),
      height_(height),
      depth_(depth),
      blocks_(
          static_cast<std::size_t>(width) *
          static_cast<std::size_t>(height) *
          static_cast<std::size_t>(depth), 0),
      lightDepths_(
          static_cast<std::size_t>(width) *
          static_cast<std::size_t>(height), 0) {

    for (int x = 0; x < width_; ++x) {
        for (int y = 0; y < depth_; ++y) {
            for (int z = 0; z < height_; ++z) {
                blocks_[index(x, y, z)] =
                    static_cast<std::uint8_t>(
                        y <= depth_ * 2 / 3 ? 1 : 0);
            }
        }
    }

    calcLightDepths(0, 0, width_, height_);
    load();
}

std::size_t Level::index(int x, int y, int z) const {
    return (
        static_cast<std::size_t>(y) *
        static_cast<std::size_t>(height_) +
        static_cast<std::size_t>(z)) *
        static_cast<std::size_t>(width_) +
        static_cast<std::size_t>(x);
}

void Level::load() {
    gzFile file = gzopen("level.dat", "rb");
    if (!file)
        return;

    std::vector<std::uint8_t> loaded(blocks_.size());

    std::size_t offset = 0;

    while (offset < loaded.size()) {
        const unsigned int remaining =
            static_cast<unsigned int>(
                std::min<std::size_t>(
                    loaded.size() - offset,
                    0x7FFFFFFFu));

        const int read = gzread(
            file,
            loaded.data() + offset,
            remaining);

        if (read <= 0)
            break;

        offset += static_cast<std::size_t>(read);
    }

    const int closeResult = gzclose(file);

    // Match DataInputStream.readFully semantics without allowing a truncated
    // save to overwrite the generated/default world with partial data.
    if (offset != loaded.size() || closeResult != Z_OK)
        return;

    blocks_.swap(loaded);
    calcLightDepths(0, 0, width_, height_);

    for (LevelListener* listener : listeners_) {
        if (listener)
            listener->allChanged();
    }
}

void Level::save() const {
    gzFile file = gzopen("level.dat", "wb");
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

        const int written = gzwrite(
            file,
            blocks_.data() + offset,
            remaining);

        if (written <= 0)
            break;

        offset += static_cast<std::size_t>(written);
    }

    gzclose(file);
}

void Level::calcLightDepths(int x0, int y0, int x1, int y1) {
    for (int x = x0; x < x0 + x1; ++x) {
        for (int z = y0; z < y0 + y1; ++z) {
            const int oldDepth =
                lightDepths_[x + z * width_];

            int y = depth_ - 1;
            while (y > 0 && !isLightBlocker(x, y, z))
                --y;

            lightDepths_[x + z * width_] = y;

            if (oldDepth == y)
                continue;

            const int yLow = std::min(oldDepth, y);
            const int yHigh = std::max(oldDepth, y);

            for (LevelListener* listener : listeners_) {
                if (listener) {
                    listener->lightColumnChanged(
                        x, z, yLow, yHigh);
                }
            }
        }
    }
}

void Level::addListener(LevelListener* listener) {
    if (!listener)
        return;

    if (std::find(
            listeners_.begin(),
            listeners_.end(),
            listener) == listeners_.end()) {
        listeners_.push_back(listener);
    }
}

void Level::removeListener(LevelListener* listener) {
    listeners_.erase(
        std::remove(
            listeners_.begin(),
            listeners_.end(),
            listener),
        listeners_.end());
}

bool Level::isTile(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ || y >= depth_ || z >= height_) {
        return false;
    }

    // The reference treats every non-zero byte as an occupied tile.
    // Keep that rule so level.dat round-trips do not reinterpret tile IDs.
    return blocks_[index(x, y, z)] != 0;
}

bool Level::isSolidTile(int x, int y, int z) const {
    return isTile(x, y, z);
}

bool Level::isLightBlocker(int x, int y, int z) const {
    return isSolidTile(x, y, z);
}

void Level::getCubes(
    const AABB& box,
    std::vector<AABB>& result) const {
    result.clear();

    int x0 = static_cast<int>(box.x0);
    int x1 = static_cast<int>(box.x1 + 1.0f);
    int y0 = static_cast<int>(box.y0);
    int y1 = static_cast<int>(box.y1 + 1.0f);
    int z0 = static_cast<int>(box.z0);
    int z1 = static_cast<int>(box.z1 + 1.0f);

    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    z0 = std::max(z0, 0);
    x1 = std::min(x1, width_);
    y1 = std::min(y1, depth_);
    z1 = std::min(z1, height_);

    for (int x = x0; x < x1; ++x) {
        for (int y = y0; y < y1; ++y) {
            for (int z = z0; z < z1; ++z) {
                if (isSolidTile(x, y, z)) {
                    result.emplace_back(
                        x, y, z,
                        x + 1, y + 1, z + 1);
                }
            }
        }
    }

}

float Level::getBrightness(int x, int y, int z) const {
    constexpr float dark = 0.8f;
    constexpr float light = 1.0f;

    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ || y >= depth_ || z >= height_) {
        return light;
    }

    return y < lightDepths_[x + z * width_]
        ? dark
        : light;
}

void Level::setTile(int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ || y >= depth_ || z >= height_) {
        return;
    }

    const std::uint8_t newType =
        static_cast<std::uint8_t>(type);

    if (blocks_[index(x, y, z)] == newType)
        return;

    blocks_[index(x, y, z)] = newType;

    calcLightDepths(x, z, 1, 1);

    for (LevelListener* listener : listeners_) {
        if (listener)
            listener->tileChanged(x, y, z);
    }
}

} // namespace luckee
