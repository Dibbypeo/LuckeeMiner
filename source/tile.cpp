#include "luckee/tile.hpp"

#include <memory>

#include "luckee/level.hpp"
#include "luckee/java_random.hpp"
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
Tile* Tile::boundary = nullptr;
Tile* Tile::waterFlowing = nullptr;
Tile* Tile::waterStill = nullptr;
Tile* Tile::lavaFlowing = nullptr;
Tile* Tile::lavaStill = nullptr;

namespace {

int randomInt(JavaRandom& random, int bound) {
    return random.nextInt(bound);
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
    JavaRandom&) {
}

void Tile::destroy(
    Level& level,
    int x, int y, int z,
    ParticleEngine& particleEngine) const {
    constexpr int SD = 4;
    constexpr std::size_t PARTICLE_COUNT = SD * SD * SD;

    // Keep each destruction effect complete; do not partially fill the pool.
    if (!particleEngine.hasCapacityFor(PARTICLE_COUNT))
        return;

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
    JavaRandom& random) {
    // Classic only updates a selected grass block on one quarter of attempts.
    if (random.nextInt(4) != 0)
        return;
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
            level.isLit(xt, yt + 1, zt)) {
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
    JavaRandom&) {
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

FluidTile::FluidTile(
    int id, int textureId,
    int flowingId, int stillId,
    int family, bool stationary)
    : Tile(id, textureId),
      flowingId_(flowingId), stillId_(stillId),
      family_(family), stationary_(stationary) {
}

bool FluidTile::hasOpenFlowNeighbor(
    const Level& level, int x, int y, int z) const {
    return level.getTile(x - 1, y, z) == 0 ||
           level.getTile(x + 1, y, z) == 0 ||
           level.getTile(x, y, z - 1) == 0 ||
           level.getTile(x, y, z + 1) == 0 ||
           level.getTile(x, y - 1, z) == 0;
}

void FluidTile::neighborChanged(
    Level& level, int x, int y, int z, int changedType) {
    // The stationary subclass reacts only to the flowing opposite fluid;
    // the flowing subclass reacts to either flowing or stationary opposite
    // fluid, matching their different Java neighborChanged overrides.
    const bool opposite =
        stationary_
            ? ((family_ == 1 && changedType == 10) ||
               (family_ == 2 && changedType == 8))
            : ((family_ == 1 && (changedType == 10 || changedType == 11)) ||
               (family_ == 2 && (changedType == 8 || changedType == 9)));
    if (opposite) {
        level.setTileRaw(x, y, z, Tile::rock->id());
        return;
    }
    if (stationary_ && hasOpenFlowNeighbor(level, x, y, z))
        level.setTileRaw(x, y, z, flowingId_);
}

bool FluidTile::flowSideways(
    Level& level, int x, int y, int z, int depth) {
    if (level.getTile(x, y, z) != 0 ||
        !level.setTile(x, y, z, flowingId_))
        return false;

    const int maxDepth = family_ == 1 ? 8 : 2;
    if (depth < maxDepth)
        return flowFrom(level, x, y, z, depth + 1);
    return false;
}

bool FluidTile::flowFrom(
    Level& level, int x, int y, int z, int depth) {
    bool moved = false;
    int flowY = y;

    while (flowY > 0 && level.getTile(x, flowY - 1, z) == 0) {
        const bool placed = level.setTile(x, flowY - 1, z, flowingId_);
        moved = moved || placed;
        if (!placed || family_ == 2)
            break;
        --flowY;
    }

    if (family_ == 1 || !moved) {
        // Evaluate all directions: Java's boolean | does not short-circuit.
        const bool west = flowSideways(level, x - 1, flowY, z, depth);
        const bool east = flowSideways(level, x + 1, flowY, z, depth);
        const bool north = flowSideways(level, x, flowY, z - 1, depth);
        const bool south = flowSideways(level, x, flowY, z + 1, depth);
        moved = moved || west || east || north || south;
    }

    if (!moved)
        level.setTileRaw(x, flowY, z, stillId_);
    return moved;
}

void FluidTile::tick(
    Level& level, int x, int y, int z, JavaRandom&) {
    if (!stationary_)
        flowFrom(level, x, y, z, 0);
}

void initializeTiles() {
    static Tile rockTile(1, 1);
    static GrassTile grassTile(2);
    static DirtTile dirtTile(3, 2);
    static Tile stoneBrickTile(4, 16);
    static Tile woodTile(5, 4);
    static BushTile bushTile(6);
    static BoundaryTile boundaryTile(7, 17);
    static FluidTile waterFlowingTile(8, 14, 8, 9, 1, false);
    static FluidTile waterStillTile(9, 14, 8, 9, 1, true);
    static FluidTile lavaFlowingTile(10, 30, 10, 11, 2, false);
    static FluidTile lavaStillTile(11, 30, 10, 11, 2, true);

    Tile::rock = &rockTile;
    Tile::grass = &grassTile;
    Tile::dirt = &dirtTile;
    Tile::stoneBrick = &stoneBrickTile;
    Tile::wood = &woodTile;
    Tile::bush = &bushTile;
    Tile::boundary = &boundaryTile;
    Tile::waterFlowing = &waterFlowingTile;
    Tile::waterStill = &waterStillTile;
    Tile::lavaFlowing = &lavaFlowingTile;
    Tile::lavaStill = &lavaStillTile;
}

} // namespace luckee
