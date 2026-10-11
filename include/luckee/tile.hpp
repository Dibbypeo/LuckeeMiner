#pragma once

#include "luckee/java_random.hpp"

#include "luckee/aabb.hpp"

namespace luckee {

class Level;
class ParticleEngine;

class Tile {
public:
    static constexpr int MAX_TILES = 256;

    static Tile* tiles[MAX_TILES];
    static Tile* rock;
    static Tile* grass;
    static Tile* dirt;
    static Tile* stoneBrick;
    static Tile* wood;
    static Tile* bush;
    static Tile* boundary;
    static Tile* waterFlowing;
    static Tile* waterStill;
    static Tile* lavaFlowing;
    static Tile* lavaStill;

    explicit Tile(int id, int textureId = 0);
    virtual ~Tile() = default;

    int id() const { return id_; }
    int texture() const { return textureId_; }

    virtual int getTexture(int face) const;
    virtual bool blocksLight() const { return true; }
    virtual bool isSolid() const { return true; }
    virtual AABB getAABB(int x, int y, int z) const;

    virtual void tick(
        Level& level,
        int x, int y, int z,
        JavaRandom& random);
    virtual void neighborChanged(
        Level& level,
        int x, int y, int z,
        int changedType) {}
    virtual bool isLiquid() const { return false; }
    virtual int liquidFamily() const { return 0; }

    virtual bool isCrossPlant() const { return false; }

    virtual void destroy(
        Level& level,
        int x, int y, int z,
        ParticleEngine& particleEngine) const;

protected:
    int id_;
    int textureId_;
};

class GrassTile final : public Tile {
public:
    explicit GrassTile(int id);

    int getTexture(int face) const override;

    void tick(
        Level& level,
        int x, int y, int z,
        JavaRandom& random) override;
};

class DirtTile final : public Tile {
public:
    DirtTile(int id, int textureId)
        : Tile(id, textureId) {}
};

class BushTile final : public Tile {
public:
    explicit BushTile(int id);

    void tick(
        Level& level,
        int x, int y, int z,
        JavaRandom& random) override;

    bool blocksLight() const override { return false; }
    bool isSolid() const override { return false; }
    AABB getAABB(int, int, int) const override;
    bool isCrossPlant() const override { return true; }
};

class BoundaryTile final : public Tile {
public:
    explicit BoundaryTile(int id, int textureId)
        : Tile(id, textureId) {}
};

class FluidTile final : public Tile {
public:
    FluidTile(int id, int textureId, int flowingId, int stillId,
              int family, bool stationary);
    bool isSolid() const override { return false; }
    bool isLiquid() const override { return true; }
    int liquidFamily() const override { return family_; }
    AABB getAABB(int x, int y, int z) const override;
    void tick(Level& level, int x, int y, int z, JavaRandom& random) override;
    void neighborChanged(Level& level, int x, int y, int z, int changedType) override;

private:
    bool flowFrom(Level& level, int x, int y, int z, int depth);
    bool flowSideways(Level& level, int x, int y, int z, int depth);
    bool hasOpenFlowNeighbor(const Level& level, int x, int y, int z) const;
    int flowingId_;
    int stillId_;
    int family_;
    bool stationary_;
};

void initializeTiles();

} // namespace luckee
