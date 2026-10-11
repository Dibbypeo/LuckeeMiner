#include "luckee/level.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <zlib.h>

#include "luckee/classic_worldgen.hpp"
#include "luckee/perlin_noise.hpp"
#include "luckee/tile.hpp"

namespace luckee {

namespace {
constexpr std::uint32_t CLASSIC_SAVE_MAGIC = 656127880u;
constexpr std::uint8_t CLASSIC_SAVE_VERSION = 1;

bool gzExact(gzFile file, void* data, std::size_t length) {
    auto* bytes = static_cast<std::uint8_t*>(data);
    std::size_t offset = 0;
    while (offset < length) {
        const unsigned int n = static_cast<unsigned int>(
            std::min<std::size_t>(length - offset, 32768u));
        const int read = gzread(file, bytes + offset, n);
        if (read <= 0) return false;
        offset += static_cast<std::size_t>(read);
    }
    return true;
}

bool gzWriteExact(gzFile file, const void* data, std::size_t length) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::size_t offset = 0;
    while (offset < length) {
        const unsigned int n = static_cast<unsigned int>(
            std::min<std::size_t>(length - offset, 32768u));
        const int written = gzwrite(file, bytes + offset, n);
        if (written <= 0) return false;
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

void append16(std::vector<std::uint8_t>& out, std::uint16_t n) {
    out.push_back(static_cast<std::uint8_t>(n >> 8));
    out.push_back(static_cast<std::uint8_t>(n));
}
void append32(std::vector<std::uint8_t>& out, std::uint32_t n) {
    out.push_back(static_cast<std::uint8_t>(n >> 24));
    out.push_back(static_cast<std::uint8_t>(n >> 16));
    out.push_back(static_cast<std::uint8_t>(n >> 8));
    out.push_back(static_cast<std::uint8_t>(n));
}
void append64(std::vector<std::uint8_t>& out, std::uint64_t n) {
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<std::uint8_t>(n >> shift));
}
void appendMutfUnit(std::vector<std::uint8_t>& out, std::uint16_t c) {
    if (c >= 1 && c <= 0x7F) out.push_back(static_cast<std::uint8_t>(c));
    else if (c <= 0x7FF) {
        out.push_back(static_cast<std::uint8_t>(0xC0 | ((c >> 6) & 0x1F)));
        out.push_back(static_cast<std::uint8_t>(0x80 | (c & 0x3F)));
    } else {
        out.push_back(static_cast<std::uint8_t>(0xE0 | ((c >> 12) & 0x0F)));
        out.push_back(static_cast<std::uint8_t>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<std::uint8_t>(0x80 | (c & 0x3F)));
    }
}
void appendUtf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
    else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}
bool appendJavaUtf(std::vector<std::uint8_t>& out, const std::string& text) {
    std::vector<std::uint8_t> encoded;
    encoded.reserve(text.size() + 8);
    for (std::size_t i = 0; i < text.size();) {
        const auto a = static_cast<std::uint8_t>(text[i]);
        std::uint32_t cp = 0xFFFD;
        std::size_t n = 1;
        if (a < 0x80) cp = a;
        else if ((a & 0xE0) == 0xC0 && i + 1 < text.size()) {
            cp = ((a & 0x1F) << 6) |
                 (static_cast<std::uint8_t>(text[i + 1]) & 0x3F);
            n = 2;
        } else if ((a & 0xF0) == 0xE0 && i + 2 < text.size()) {
            cp = ((a & 0x0F) << 12) |
                 ((static_cast<std::uint8_t>(text[i + 1]) & 0x3F) << 6) |
                 (static_cast<std::uint8_t>(text[i + 2]) & 0x3F);
            n = 3;
        } else if ((a & 0xF8) == 0xF0 && i + 3 < text.size()) {
            cp = ((a & 7) << 18) |
                 ((static_cast<std::uint8_t>(text[i + 1]) & 0x3F) << 12) |
                 ((static_cast<std::uint8_t>(text[i + 2]) & 0x3F) << 6) |
                 (static_cast<std::uint8_t>(text[i + 3]) & 0x3F);
            n = 4;
        }
        i += n;
        if (cp <= 0xFFFF) appendMutfUnit(encoded, static_cast<std::uint16_t>(cp));
        else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            appendMutfUnit(encoded, static_cast<std::uint16_t>(0xD800 + (cp >> 10)));
            appendMutfUnit(encoded, static_cast<std::uint16_t>(0xDC00 + (cp & 0x3FF)));
        } else appendMutfUnit(encoded, 0xFFFD);
        if (encoded.size() > 65535u) return false;
    }
    append16(out, static_cast<std::uint16_t>(encoded.size()));
    out.insert(out.end(), encoded.begin(), encoded.end());
    return true;
}
bool read16(gzFile file, std::uint16_t& value) {
    std::uint8_t b[2];
    if (!gzExact(file, b, 2)) return false;
    value = (static_cast<std::uint16_t>(b[0]) << 8) | b[1];
    return true;
}
bool read32(gzFile file, std::uint32_t& value) {
    std::uint8_t b[4];
    if (!gzExact(file, b, 4)) return false;
    value = (static_cast<std::uint32_t>(b[0]) << 24) |
            (static_cast<std::uint32_t>(b[1]) << 16) |
            (static_cast<std::uint32_t>(b[2]) << 8) | b[3];
    return true;
}
bool read64(gzFile file, std::uint64_t& value) {
    std::uint8_t b[8];
    if (!gzExact(file, b, 8)) return false;
    value = 0;
    for (std::uint8_t c : b) value = (value << 8) | c;
    return true;
}
bool readJavaUtf(gzFile file, std::string& output) {
    std::uint16_t length = 0;
    if (!read16(file, length)) return false;
    std::vector<std::uint8_t> bytes(length);
    if (length && !gzExact(file, bytes.data(), length)) return false;
    std::vector<std::uint16_t> units;
    units.reserve(length);
    for (std::size_t i = 0; i < bytes.size();) {
        const std::uint8_t a = bytes[i++];
        if ((a & 0x80) == 0) {
            if (a == 0) return false;
            units.push_back(a);
        } else if ((a & 0xE0) == 0xC0) {
            if (i >= bytes.size() || (bytes[i] & 0xC0) != 0x80) return false;
            units.push_back(static_cast<std::uint16_t>(((a & 0x1F) << 6) | (bytes[i++] & 0x3F)));
        } else if ((a & 0xF0) == 0xE0) {
            if (i + 1 >= bytes.size() || (bytes[i] & 0xC0) != 0x80 ||
                (bytes[i + 1] & 0xC0) != 0x80) return false;
            units.push_back(static_cast<std::uint16_t>(
                ((a & 0x0F) << 12) | ((bytes[i] & 0x3F) << 6) | (bytes[i + 1] & 0x3F)));
            i += 2;
        } else return false;
    }
    output.clear();
    for (std::size_t i = 0; i < units.size(); ++i) {
        std::uint32_t cp = units[i];
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 1 >= units.size() || units[i + 1] < 0xDC00 || units[i + 1] > 0xDFFF)
                return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (units[++i] - 0xDC00);
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) return false;
        appendUtf8(output, cp);
    }
    return true;
}
std::uint16_t be16(const std::uint8_t b[2]) {
    return (static_cast<std::uint16_t>(b[0]) << 8) | b[1];
}
std::uint32_t be32(const std::uint8_t b[4]) {
    return (static_cast<std::uint32_t>(b[0]) << 24) |
           (static_cast<std::uint32_t>(b[1]) << 16) |
           (static_cast<std::uint32_t>(b[2]) << 8) | b[3];
}
} // namespace

Level::Level(
    int width, int height, int depth,
    ClassicGenerationProgress progress)
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
      random_(),
      randomTickState_(static_cast<std::uint32_t>(random_.nextInt())) {
    initializeTiles();

    const bool mapLoaded = load();

    if (!mapLoaded)
        generateMap(progress);

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

void Level::generateMap(ClassicGenerationProgress progress) {
    worldName_ = "A Nice World";
    creator_ = "LuckeeMiner";
    creationTimeMs_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    // The Classic generator owns a separate stream for noise construction,
    // cave carving and water/lava source placement.
    JavaRandom generationRandom;
    blocks_ = generateClassicWorld(
        width_, height_, depth_, generationRandom, progress);
}

void Level::generateNewWorld(ClassicGenerationProgress progress) {
    unprocessed_ = 0;
    random_ = JavaRandom();
    randomTickState_ = static_cast<std::uint32_t>(random_.nextInt());
    generateMap(progress);
    calcLightDepths(0, 0, width_, height_);
    for (LevelListener* listener : listeners_) {
        if (listener)
            listener->allChanged();
    }
}

bool Level::load() {
    const char* const paths[] = {"level.dat", "level.dat.bak"};
    for (const char* path : paths) {
        gzFile file = gzopen(path, "rb");
        if (!file) continue;

        bool valid = true;
        std::uint8_t first[4]{};
        valid = gzExact(file, first, sizeof(first));
        std::vector<std::uint8_t> loaded(blocks_.size(), 0);
        std::string loadedName = "--";
        std::string loadedCreator = "unknown";
        std::uint64_t loadedTime = 0;
        bool hasMetadata = false;

        if (valid && be32(first) == CLASSIC_SAVE_MAGIC) {
            hasMetadata = true;
            std::uint8_t version = 0;
            std::uint32_t width = 0, height = 0, depth = 0;
            valid = gzExact(file, &version, 1) &&
                    version <= CLASSIC_SAVE_VERSION &&
                    readJavaUtf(file, loadedName) &&
                    readJavaUtf(file, loadedCreator) &&
                    read64(file, loadedTime);

            if (valid) {
                std::uint8_t dimensionPrefix[2]{};
                valid = gzExact(file, dimensionPrefix, sizeof(dimensionPrefix));
                if (valid &&
                    be16(dimensionPrefix) == static_cast<std::uint16_t>(width_)) {
                    // Original on-disk order matches native X, Z, Y dimensions.
                    std::uint16_t savedZ = 0;
                    std::uint16_t savedY = 0;
                    width = be16(dimensionPrefix);
                    valid = read16(file, savedZ) && read16(file, savedY);
                    height = savedZ;
                    depth = savedY;
                } else if (valid) {
                    // Support earlier development saves with three 32-bit dimensions.
                    std::uint8_t dimensionBytes[12]{};
                    dimensionBytes[0] = dimensionPrefix[0];
                    dimensionBytes[1] = dimensionPrefix[1];
                    valid = gzExact(file, dimensionBytes + 2, 10);
                    if (valid) {
                        width = be32(dimensionBytes);
                        height = be32(dimensionBytes + 4);
                        depth = be32(dimensionBytes + 8);
                    }
                }
            }

            if (valid &&
                (width != static_cast<std::uint32_t>(width_) ||
                 height != static_cast<std::uint32_t>(height_) ||
                 depth != static_cast<std::uint32_t>(depth_)))
                valid = false;
        } else if (valid) {
            // Legacy format contains the raw block bytes directly.
            if (loaded.size() < sizeof(first)) valid = false;
            else {
                std::copy(first, first + sizeof(first), loaded.begin());
                valid = gzExact(
                    file, loaded.data() + sizeof(first),
                    loaded.size() - sizeof(first));
            }
        }

        if (valid && hasMetadata)
            valid = gzExact(file, loaded.data(), loaded.size());

        std::uint8_t extra = 0;
        const int extraRead = valid ? gzread(file, &extra, 1) : -1;
        const int closeResult = gzclose(file);
        if (!valid || extraRead != 0 || closeResult != Z_OK) continue;

        blocks_.swap(loaded);
        if (hasMetadata) {
            worldName_ = loadedName;
            creator_ = loadedCreator;
            creationTimeMs_ = loadedTime;
        } else {
            worldName_ = "--";
            creator_ = "unknown";
            creationTimeMs_ = 0;
        }
        calcLightDepths(0, 0, width_, height_);
        for (LevelListener* listener : listeners_)
            if (listener) listener->allChanged();
        return true;
    }
    return false;
}

bool Level::save() const {
    constexpr const char* SAVE_PATH = "level.dat";
    constexpr const char* TEMP_PATH = "level.dat.tmp";
    constexpr const char* BACKUP_PATH = "level.dat.bak";

    if (width_ <= 0 || width_ > 32767 ||
        height_ <= 0 || height_ > 32767 ||
        depth_ <= 0 || depth_ > 32767) {
        std::fputs("level.dat: dimensions do not fit Classic's signed 16-bit fields.\n", stderr);
        return false;
    }

    std::vector<std::uint8_t> header;
    header.reserve(64 + worldName_.size() + creator_.size());
    append32(header, CLASSIC_SAVE_MAGIC);
    header.push_back(CLASSIC_SAVE_VERSION);
    if (!appendJavaUtf(header, worldName_) ||
        !appendJavaUtf(header, creator_)) {
        std::fputs("level.dat: world metadata exceeds Java's UTF limit.\n", stderr);
        return false;
    }
    append64(header, creationTimeMs_);
    // Original field order is width (X), Z dimension, Y dimension.
    append16(header, static_cast<std::uint16_t>(width_));
    append16(header, static_cast<std::uint16_t>(height_));
    append16(header, static_cast<std::uint16_t>(depth_));

    gzFile file = gzopen(TEMP_PATH, "wb1");
    if (!file) {
        std::perror(TEMP_PATH);
        return false;
    }
    bool ok = gzWriteExact(file, header.data(), header.size());
    std::size_t offset = 0;
    while (ok && offset < blocks_.size()) {
        const std::size_t count =
            std::min<std::size_t>(blocks_.size() - offset, 32768u);
        ok = gzWriteExact(file, blocks_.data() + offset, count);
        offset += count;
    }
    const int closeResult = gzclose(file);
    if (!ok || closeResult != Z_OK) {
        std::fputs("level.dat: could not complete compressed Classic save.\n", stderr);
        std::remove(TEMP_PATH);
        return false;
    }

    bool hadPrevious = false;
    std::FILE* previous = std::fopen(SAVE_PATH, "rb");
    if (previous) {
        hadPrevious = true;
        std::fclose(previous);
        std::remove(BACKUP_PATH);
        if (std::rename(SAVE_PATH, BACKUP_PATH) != 0) {
            std::perror("level.dat: could not preserve previous save");
            std::remove(TEMP_PATH);
            return false;
        }
    }
    if (std::rename(TEMP_PATH, SAVE_PATH) != 0) {
        std::perror("level.dat: could not finalize save");
        if (hadPrevious && std::rename(BACKUP_PATH, SAVE_PATH) != 0)
            std::fputs("level.dat: previous save remains in level.dat.bak.\n", stderr);
        std::remove(TEMP_PATH);
        return false;
    }
    std::remove(BACKUP_PATH);
    return true;
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

            // Classic stores one above the highest light-blocking tile.
            lightDepths_[
                x + z * width_] = y + 1;

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

    const int x0 = static_cast<int>(std::floor(box.x0));
    const int x1 = static_cast<int>(std::floor(box.x1 + 1.0f));
    const int y0 = static_cast<int>(std::floor(box.y0));
    const int y1 = static_cast<int>(std::floor(box.y1 + 1.0f));
    const int z0 = static_cast<int>(std::floor(box.z0));
    const int z1 = static_cast<int>(std::floor(box.z1 + 1.0f));

    for (int x = x0; x < x1; ++x) {
        for (int y = y0; y < y1; ++y) {
            for (int z = z0; z < z1; ++z) {
                const bool insideWorld =
                    x >= 0 && x < width_ &&
                    y >= 0 && y < depth_ &&
                    z >= 0 && z < height_;

                if (!insideWorld) {
                    // Classic provides the special boundary tile around the
                    // horizontal world edges and below the world, but not a
                    // ceiling above the topmost build layer.
                    const bool boundary =
                        x < 0 || x >= width_ ||
                        z < 0 || z >= height_ || y < 0;
                    if (boundary && Tile::boundary)
                        result.push_back(
                            Tile::boundary->getAABB(x, y, z));
                    continue;
                }

                const int id = static_cast<int>(
                    blocks_[index(x, y, z)]);
                Tile* tile =
                    id >= 0 && id < Tile::MAX_TILES
                        ? Tile::tiles[id]
                        : nullptr;
                if (tile && tile->isSolid())
                    result.push_back(tile->getAABB(x, y, z));
            }
        }
    }
}

bool Level::setTileRaw(
    int x, int y, int z, int type) {
    if (x < 0 || y < 0 || z < 0 ||
        x >= width_ || y >= depth_ || z >= height_)
        return false;

    const std::uint8_t newType = static_cast<std::uint8_t>(type);
    const std::size_t tileIndex = index(x, y, z);
    if (blocks_[tileIndex] == newType)
        return false;

    blocks_[tileIndex] = newType;
    calcLightDepths(x, z, 1, 1);
    for (LevelListener* listener : listeners_) {
        if (listener)
            listener->tileChanged(x, y, z);
    }
    return true;
}

bool Level::setTile(int x, int y, int z, int type) {
    if (!setTileRaw(x, y, z, type))
        return false;

    static constexpr int DX[6] = {-1, 1, 0, 0, 0, 0};
    static constexpr int DY[6] = {0, 0, -1, 1, 0, 0};
    static constexpr int DZ[6] = {0, 0, 0, 0, -1, 1};
    const int changedType = static_cast<std::uint8_t>(type);

    for (int side = 0; side < 6; ++side) {
        const int nx = x + DX[side];
        const int ny = y + DY[side];
        const int nz = z + DZ[side];
        if (nx < 0 || ny < 0 || nz < 0 ||
            nx >= width_ || ny >= depth_ || nz >= height_)
            continue;

        const int neighborId = getTile(nx, ny, nz);
        if (neighborId >= 0 && neighborId < Tile::MAX_TILES) {
            Tile* neighbor = Tile::tiles[neighborId];
            if (neighbor)
                neighbor->neighborChanged(*this, nx, ny, nz, changedType);
        }
    }
    return true;
}

bool Level::canOccupyEntityBox(
    const AABB& box, float dx, float dy, float dz) const {
    AABB candidate = box;
    candidate.move(dx, dy, dz);

    std::vector<AABB> collisions;
    getCubes(candidate, collisions);
    for (const AABB& collision : collisions) {
        if (candidate.intersects(collision))
            return false;
    }

    const int x0 = static_cast<int>(std::floor(candidate.x0));
    const int x1 = static_cast<int>(std::floor(candidate.x1 + 1.0f));
    const int y0 = static_cast<int>(std::floor(candidate.y0));
    const int y1 = static_cast<int>(std::floor(candidate.y1 + 1.0f));
    const int z0 = static_cast<int>(std::floor(candidate.z0));
    const int z1 = static_cast<int>(std::floor(candidate.z1 + 1.0f));

    for (int x = std::max(0, x0); x < std::min(width_, x1); ++x) {
        for (int y = std::max(0, y0); y < std::min(depth_, y1); ++y) {
            for (int z = std::max(0, z0); z < std::min(height_, z1); ++z) {
                const int id = getTile(x, y, z);
                if (id > 0 && id < Tile::MAX_TILES) {
                    const Tile* tile = Tile::tiles[id];
                    if (tile && tile->isLiquid())
                        return false;
                }
            }
        }
    }
    return true;
}

bool Level::containsLiquid(const AABB& box, int family) const {
    const int x0 = static_cast<int>(std::floor(box.x0));
    const int x1 = static_cast<int>(std::floor(box.x1 + 1.0f));
    const int y0 = static_cast<int>(std::floor(box.y0));
    const int y1 = static_cast<int>(std::floor(box.y1 + 1.0f));
    const int z0 = static_cast<int>(std::floor(box.z0));
    const int z1 = static_cast<int>(std::floor(box.z1 + 1.0f));

    for (int x = std::max(0, x0); x < std::min(width_, x1); ++x) {
        for (int y = std::max(0, y0); y < std::min(depth_, y1); ++y) {
            for (int z = std::max(0, z0); z < std::min(height_, z1); ++z) {
                const int id = getTile(x, y, z);
                if (id >= 0 && id < Tile::MAX_TILES) {
                    const Tile* tile = Tile::tiles[id];
                    if (tile && tile->isLiquid() &&
                        tile->liquidFamily() == family)
                        return true;
                }
            }
        }
    }
    return false;
}

void Level::tick() {
    unprocessed_ +=
        static_cast<std::uint64_t>(width_) *
        static_cast<std::uint64_t>(height_) *
        static_cast<std::uint64_t>(depth_);

    const std::uint64_t ticks = unprocessed_ / TILE_UPDATE_INTERVAL;
    unprocessed_ -= ticks * TILE_UPDATE_INTERVAL;

    for (std::uint64_t i = 0; i < ticks; ++i) {
        // Coordinates are selected with the original 32-bit LCG, separately
        // from java.util.Random, which supplies the tile tick callbacks.
        randomTickState_ =
            randomTickState_ * 1664525u + 1013904223u;
        const int x = static_cast<int>(
            (randomTickState_ >> 16) &
            static_cast<std::uint32_t>(width_ - 1));

        randomTickState_ =
            randomTickState_ * 1664525u + 1013904223u;
        const int y = static_cast<int>(
            (randomTickState_ >> 16) &
            static_cast<std::uint32_t>(depth_ - 1));

        randomTickState_ =
            randomTickState_ * 1664525u + 1013904223u;
        const int z = static_cast<int>(
            (randomTickState_ >> 16) &
            static_cast<std::uint32_t>(height_ - 1));

        const int id = getTile(x, y, z);
        if (id >= 0 && id < Tile::MAX_TILES) {
            Tile* tile = Tile::tiles[id];
            if (tile && tile->isTickable())
                tile->tick(*this, x, y, z, random_);
        }
    }
}

} // namespace luckee
