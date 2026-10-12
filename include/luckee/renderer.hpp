#pragma once

#include <3ds.h>
#include <citro3d.h>

#include <cstdint>
#include <string>
#include <vector>

#include "luckee/frustum.hpp"
#include "luckee/hit_result.hpp"
#include "luckee/level_listener.hpp"
#include "luckee/particle_engine.hpp"
#include "luckee/zombie.hpp"

namespace luckee {

class Level;
class Player;

class Renderer final : public LevelListener {
public:
    bool initialize();
    void shutdown();

    void setLevel(const Level& level) { level_ = &level; }

    void render(
        const Level& level,
        const Player& player,
        const std::vector<Zombie>& zombies,
        const ParticleEngine& particleEngine,
        int selectedTileId,
        float alpha,
        bool placingMode = false);

    const std::string& error() const { return error_; }

    bool pick(const Level& level, const Player& player, float alpha);

    const HitResult* hitResult() const {
        return hasHit_ ? &hitResult_ : nullptr;
    }

    void tileChanged(int x, int y, int z) override;
    void lightColumnChanged(
        int x, int z, int y0, int y1) override;
    void allChanged() override;

private:
    struct Vertex {
        float x, y, z;
        float r, g, b, a;
        float u, v;
    };

    struct ChunkMesh {
        int minX = 0;
        int minY = 0;
        int minZ = 0;
        int maxX = 0;
        int maxY = 0;
        int maxZ = 0;

        AABB bounds{0, 0, 0, 0, 0, 0};

        void* vbo[2] = {nullptr, nullptr};
        int vertexCount[2] = {0, 0};
        void* waterVbo[2] = {nullptr, nullptr};
        int waterVertexCount[2] = {0, 0};
        void* lavaVbo[2] = {nullptr, nullptr};
        int lavaVertexCount[2] = {0, 0};
        bool dirty = true;
        std::uint64_t dirtiedTimeMs = 0;
    };

    void initializeChunks(const Level& level);

    bool rebuildChunk(
        const Level& level,
        ChunkMesh& chunk);

    void appendFace(
        std::vector<Vertex>& vertices,
        int x, int y, int z,
        int face,
        float brightness,
        int textureId) const;

    void appendBush(
        std::vector<Vertex>& vertices,
        int x, int y, int z,
        float brightness) const;

    void appendFluidFace(
        std::vector<Vertex>& vertices,
        int x, int y, int z,
        int face, float brightness, int textureId) const;

    void drawChunk(
        const ChunkMesh& chunk,
        int layer);

    void drawWaterChunk(
        const ChunkMesh& chunk,
        int layer);
    void drawLavaChunk(
        const ChunkMesh& chunk,
        int layer);
    void configureFog(
        const Level& level,
        const Player& player,
        bool litLayer);

    void renderZombies(
        const std::vector<Zombie>& zombies,
        const Player& player,
        float alpha,
        bool litLayer);

    void renderParticles(
        const ParticleEngine& particleEngine,
        const Player& player,
        float alpha,
        bool litLayer);

    void renderHud(
        int selectedTileId);

    void renderHorizon(const Level& level);

    void appendCharacterCube(
        std::vector<Vertex>& vertices,
        const Zombie& zombie,
        const CharacterPart& part,
        double time,
        float alpha,
        float brightness) const;

    void releaseChunkMesh(
        ChunkMesh& chunk);

    bool ensureCharacterVboCapacity(
        std::size_t required);

    bool ensureParticleVboCapacity(
        std::size_t required);
    bool ensureHudVboCapacity(
        std::size_t required);

    void markDirtyRange(
        const Level& level,
        int x0, int y0, int z0,
        int x1, int y1, int z1);

    std::vector<ChunkMesh> chunks_;
    std::vector<unsigned char> visible_;
    int chunkAmountX_ = 0;
    int chunkAmountY_ = 0;
    int chunkAmountZ_ = 0;
    bool chunksInitialized_ = false;

    const Level* level_ = nullptr;

    C3D_RenderTarget* target_ = nullptr;
    DVLB_s* shaderDvlb_ = nullptr;
    shaderProgram_s program_{};
    C3D_Mtx projection_{};
    C3D_Tex terrainTexture_{};
    C3D_Tex characterTexture_{};
    C3D_Tex rockTexture_{};
    C3D_Tex horizonWaterTexture_{};
    void* horizonRockVbo_ = nullptr;
    std::size_t horizonRockVertexCount_ = 0;
    void* horizonWaterVbo_ = nullptr;
    std::size_t horizonWaterVertexCount_ = 0;
    int horizonWidth_ = 0;
    int horizonLength_ = 0;
    C3D_FogLut dayFogLut_{};
    C3D_FogLut shadowFogLut_{};
    C3D_FogLut waterFogLut_{};
    C3D_FogLut lavaFogLut_{};

    void* characterVbo_ = nullptr;
    std::size_t characterVboCapacity_ = 0;
    int characterVertexCount_ = 0;

    void* particleVbo_ = nullptr;
    std::size_t particleVboCapacity_ = 0;

    void* hudVbo_ = nullptr;
    std::size_t hudVboCapacity_ = 0;

    Frustum frustum_;

    // Reused between chunk rebuilds to avoid repeated heap allocations.
    std::vector<Vertex> buildVertices_[2];
    std::vector<Vertex> buildWaterVertices_[2];
    std::vector<Vertex> buildLavaVertices_[2];
    std::vector<Vertex> characterVertices_;
    std::vector<Vertex> particleVertices_;
    std::vector<Vertex> hudVertices_;

    int projectionLocation_ = -1;
    int modelViewLocation_ = -1;

    void renderHit(bool placingMode);

    std::uint64_t hudWindowStartMs_ = 0;
    std::uint32_t hudFrameCount_ = 0;
    int hudFps_ = 0;
    int hudChunkUpdates_ = 0;
    int hudChunkUpdatesWindow_ = 0;

    bool hasHit_ = false;
    HitResult hitResult_{};

    bool terrainTextureLoaded_ = false;
    bool characterTextureLoaded_ = false;
    bool rockTextureLoaded_ = false;
    bool horizonWaterTextureLoaded_ = false;
    bool initialized_ = false;

    std::string error_;
    bool frameActive_ = false;
};

} // namespace luckee
