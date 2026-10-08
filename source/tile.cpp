#include "luckee/tile.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include "luckee/level.hpp"
#include "luckee/particle.hpp"
#include "luckee/particle_engine.hpp"

namespace luckee {

Tile* Tile::tiles[Tile::MAX_TILES] = {};
Tile* Tile::rock = nullptr;
Tile* Tile::grass = nullptr;
Tile* Tile::dirt = nullptr;
Tile* Tile::stoneBrick = nullptr;
Tile* Tile::wood = nullptr;
Tile* Tile::bush = nullptr;

namespace {

int randomInt(std::mt19937& random, int bound) {
    if (bound <= 0)
        return 0;
    return static_cast<int>(
        random() % static_cast<std::mt19937::result_type>(bound));
}

} // namespace

Tile::Tile(int id, int textureId)
    : id_(id),
      textureId_(textureId) {
    if (id_ >= 0 && id_ < MAX_TILES)
        tiles[id_] = this;
}

int Tile::getTexture(int) const {
    return textureId_;
}

AABB Tile::getAABB(int x, int y, int z) const {
    return AABB(
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(z),
        static_cast<float>(x + 1),
        static_cast<float>(y + 1),
        static_cast<float>(z + 1));
}

void Tile::tick(
    Level&,
    int, int, int,
    std::mt19937&) {
}

void Tile::destroy(
    Level&,
    int x, int y, int z,
    ParticleEngine& particleEngine) const {
    constexpr int SD = 4;

    for (int xx = 0; xx < SD; ++xx) {
        for (int yy = 0; yy < SD; ++yy) {
            for (int zz = 0; zz < SD; ++zz) {
                const float xp =
                    static_cast<float>(x) +
                    (static_cast<float>(xx) + 0.5f) / SD;
                const float yp =
                    static_cast<float>(y) +
                    (static_cast<float>(yy) + 0.5f) / SD;
                const float zp =
                    static_cast<float>(z) +
                    (static_cast<float>(zz) + 0.5f) / SD;

                particleEngine.add(
                    std::make_unique<Particle>(
                        level,
                        xp, yp, zp,
                        xp - x - 0.5f,
                        yp - y - 0.5f,
                        zp - z - 0.5f,
                        textureId_));
            }
        }
    }
}

GrassTile::GrassTile(int id)
    : Tile(id, 3) {
}

int GrassTile::getTexture(int face) const {
    if (face == 1)
        return 0;
    if (face == 0)
        return 2;
    return 3;
}

void GrassTile::tick(
    Level& level,
    int x, int y, int z,
    std::mt19937& random) {
    if (!level.isLit(x, y, z)) {
        level.setTile(
            x, y, z,
            dirt->id());
        return;
    }

    for (int i = 0; i < 4; ++i) {
        const int xt =
            x + randomInt(random, 3) - 1;
        const int yt =
            y + randomInt(random, 5) - 3;
        const int zt =
            z + randomInt(random, 3) - 1;

        if (level.getTile(xt, yt, zt) == dirt->id() &&
            level.isLit(xt, yt, zt)) {
            level.setTile(
                xt, yt, zt,
                grass->id());
        }
    }
}

BushTile::BushTile(int id)
    : Tile(id, 15) {
}

void BushTile::tick(
    Level& level,
    int x, int y, int z,
    std::mt19937&) {
    const int below =
        level.getTile(x, y - 1, z);

    if (!level.isLit(x, y, z) ||
        (below != dirt->id() &&
         below != grass->id())) {
        level.setTile(x, y, z, 0);
    }
}

AABB BushTile::getAABB(int, int, int) const {
    // The historical bush has no collision volume.
    return AABB(0, 0, 0, 0, 0, 0);
}

void initializeTiles() {
    static Tile rockTile(1, 1);
    static GrassTile grassTile(2);
    static DirtTile dirtTile(3, 2);
    static Tile stoneBrickTile(4, 16);
    static Tile woodTile(5, 4);
    static BushTile bushTile(6);

    rock = &rockTile;
    grass = &grassTile;
    dirt = &dirtTile;
    stoneBrick = &stoneBrickTile;
    wood = &woodTile;
    bush = &bushTile;
}

} // namespace luckee
