#pragma once

#include <3ds.h>
#include <citro3d.h>

#include <string>
#include <vector>

#include "luckee/frustum.hpp"
#include "luckee/level_listener.hpp"

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
        float alpha);

    const std::string& error() const { return error_; }

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
        bool dirty = true;
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

    void drawChunk(
        const ChunkMesh& chunk,
        int layer);

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
    C3D_FogLut fogLut_{};

    Frustum frustum_;

    int projectionLocation_ = -1;
    int modelViewLocation_ = -1;

    bool terrainTextureLoaded_ = false;
    bool initialized_ = false;

    std::string error_;
};

} // namespace luckee
