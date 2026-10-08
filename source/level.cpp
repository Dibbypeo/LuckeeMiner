#include "luckee/level.hpp"

#include <algorithm>
#include <cstdio>
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
      random_() {
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
    // The backup is only considered if the primary save cannot be opened or
    // fails validation. This recovers from interruption during save replacement.
    const char* const paths[] = {
        "level.dat",
        "level.dat.bak"
    };

    for (const char* path : paths) {
        gzFile file = gzopen(path, "rb");
        if (!file)
            continue;

        std::vector<std::uint8_t> loaded(blocks_.size());
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

            offset += static_cast<std::size_t>(read);
        }

        // Read once beyond the expected payload to force zlib to validate the
        // GZIP trailer (CRC and uncompressed size) and reject extra data.
        unsigned char extra = 0;
        const int extraRead =
            offset == loaded.size()
                ? gzread(file, &extra, 1)
                : -1;

        const int closeResult = gzclose(file);

        if (offset != loaded.size() ||
            extraRead != 0 ||
            closeResult != Z_OK) {
            continue;
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

    return false;
}

bool Level::save() const {
    constexpr const char* SAVE_PATH = "level.dat";
    constexpr const char* TEMP_PATH = "level.dat.tmp";
    constexpr const char* BACKUP_PATH = "level.dat.bak";

    if (blocks_.size() > 0xFFFFFFFFu) {
        std::fputs(
            "level.dat: world is too large for the historical GZIP format.\n",
            stderr);
        return false;
    }

    // Do not truncate the last good save before the new file is complete.
    // The temporary file lives beside level.dat so replacement stays on the
    // same filesystem.
    std::FILE* file = std::fopen(TEMP_PATH, "wb");

    if (!file) {
        std::perror(TEMP_PATH);
        return false;
    }

    // java.util.zip.GZIPOutputStream's historical fixed header. The DEFLATE
    // stream itself may vary by zlib version; the uncompressed payload and
    // standard GZIP framing are what provide save-file compatibility.
    const unsigned char header[10] = {
        0x1F, 0x8B, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };

    if (std::fwrite(header, 1, sizeof(header), file) != sizeof(header)) {
        std::perror(TEMP_PATH);
        std::fclose(file);
        std::remove(TEMP_PATH);
        return false;
    }

    z_stream stream{};
    if (deflateInit2(
            &stream,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -MAX_WBITS,
            8,
            Z_DEFAULT_STRATEGY) != Z_OK) {
        std::fputs(
            "level.dat: could not initialize compression.\n",
            stderr);
        std::fclose(file);
        std::remove(TEMP_PATH);
        return false;
    }

    constexpr std::size_t INPUT_CHUNK = 32768;
    constexpr std::size_t OUTPUT_CHUNK = 32768;
    unsigned char output[OUTPUT_CHUNK]{};

    uLong crc = crc32(0L, Z_NULL, 0);
    std::size_t offset = 0;
    bool ok = true;

    while (offset < blocks_.size()) {
        const std::size_t remaining = blocks_.size() - offset;
        const std::size_t inputSize =
            std::min(remaining, INPUT_CHUNK);

        crc = crc32(
            crc,
            blocks_.data() + offset,
            static_cast<uInt>(inputSize));

        stream.next_in =
            const_cast<Bytef*>(
                reinterpret_cast<const Bytef*>(
                    blocks_.data() + offset));
        stream.avail_in = static_cast<uInt>(inputSize);
        offset += inputSize;

        while (stream.avail_in > 0) {
            const uInt inputBefore = stream.avail_in;

            stream.next_out = output;
            stream.avail_out = OUTPUT_CHUNK;

            const int result = deflate(&stream, Z_NO_FLUSH);

            const std::size_t produced =
                OUTPUT_CHUNK - stream.avail_out;

            if (result != Z_OK ||
                (stream.avail_in == inputBefore && produced == 0)) {
                // Avoid an endless loop if the compression library reports
                // success but neither consumes input nor produces output.
                ok = false;
                break;
            }

            if (produced != 0 &&
                std::fwrite(output, 1, produced, file) != produced) {
                ok = false;
                break;
            }
        }

        if (!ok)
            break;
    }

    if (ok) {
        int result = Z_OK;

        do {
            stream.next_in = Z_NULL;
            stream.avail_in = 0;
            stream.next_out = output;
            stream.avail_out = OUTPUT_CHUNK;

            result = deflate(&stream, Z_FINISH);

            if (result != Z_OK && result != Z_STREAM_END) {
                ok = false;
                break;
            }

            const std::size_t produced =
                OUTPUT_CHUNK - stream.avail_out;

            if (produced != 0 &&
                std::fwrite(output, 1, produced, file) != produced) {
                ok = false;
                break;
            }

            if (result == Z_OK && produced == 0) {
                // Z_FINISH must either emit bytes or finish the stream.
                ok = false;
                break;
            }
        } while (result != Z_STREAM_END);
    }

    if (deflateEnd(&stream) != Z_OK)
        ok = false;

    if (ok) {
        const unsigned char trailer[8] = {
            static_cast<unsigned char>(crc & 0xFFu),
            static_cast<unsigned char>((crc >> 8) & 0xFFu),
            static_cast<unsigned char>((crc >> 16) & 0xFFu),
            static_cast<unsigned char>((crc >> 24) & 0xFFu),
            static_cast<unsigned char>(
                static_cast<std::uint32_t>(blocks_.size()) & 0xFFu),
            static_cast<unsigned char>(
                (static_cast<std::uint32_t>(blocks_.size()) >> 8) & 0xFFu),
            static_cast<unsigned char>(
                (static_cast<std::uint32_t>(blocks_.size()) >> 16) & 0xFFu),
            static_cast<unsigned char>(
                (static_cast<std::uint32_t>(blocks_.size()) >> 24) & 0xFFu)
        };

        if (std::fwrite(trailer, 1, sizeof(trailer), file) != sizeof(trailer))
            ok = false;
    }

    if (std::fclose(file) != 0)
        ok = false;

    if (!ok) {
        std::fputs(
            "level.dat: failed while writing the temporary world save; "
            "the previous save was left untouched.\n",
            stderr);
        std::remove(TEMP_PATH);
        return false;
    }

    bool hadPreviousSave = false;
    std::FILE* previous = std::fopen(SAVE_PATH, "rb");

    if (previous) {
        hadPreviousSave = true;
        std::fclose(previous);

        // On filesystems that cannot rename over an existing file, move the
        // current save aside only after the replacement has been written.
        // If the final rename fails, restore this backup.
        std::remove(BACKUP_PATH);

        if (std::rename(SAVE_PATH, BACKUP_PATH) != 0) {
            std::perror("level.dat: could not preserve previous save");
            std::remove(TEMP_PATH);
            return false;
        }
    }

    if (std::rename(TEMP_PATH, SAVE_PATH) != 0) {
        std::perror("level.dat: could not finalize save");

        if (hadPreviousSave &&
            std::rename(BACKUP_PATH, SAVE_PATH) != 0) {
            std::fputs(
                "level.dat: the previous save remains in level.dat.bak.\n",
                stderr);
        }

        std::remove(TEMP_PATH);
        return false;
    }

    // The primary path now contains a complete validated-format save.
    // Keep a backup only if it was needed for a failed replacement.
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
                    static_cast<int>(
                        blocks_[index(x, y, z)]);

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

    const std::size_t tileIndex =
        index(x, y, z);

    if (blocks_[tileIndex] ==
        newType) {
        return false;
    }

    blocks_[tileIndex] =
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
            random_.nextInt(width_);
        const int y =
            random_.nextInt(depth_);
        const int z =
            random_.nextInt(height_);

        const int id =
            static_cast<int>(
                blocks_[index(x, y, z)]);

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
