#pragma once

#include <3ds.h>
#include <citro3d.h>

#include <vector>

namespace luckee {

class Level;
class Player;

class Renderer {
public:
    bool initialize();
    void shutdown();
    void render(const Level& level, const Player& player);

private:
    struct Vertex {
        float x, y, z;
        float r, g, b, a;
    };

    struct ChunkMesh {
        int minX = 0;
        int minY = 0;
        int minZ = 0;
        int maxX = 0;
        int maxY = 0;
        int maxZ = 0;

        void* vbo[2] = {nullptr, nullptr};
        int vertexCount[2] = {0, 0};
        bool dirty = true;
    };

    void initializeChunks(const Level& level);
    bool rebuildChunk(const Level& level, ChunkMesh& chunk);
    void appendFace(
        std::vector<Vertex>& vertices,
        int x, int y, int z,
        int face,
        float brightness) const;
    void drawChunk(const ChunkMesh& chunk, int layer);

    std::vector<ChunkMesh> chunks_;
    int chunkAmountX_ = 0;
    int chunkAmountY_ = 0;
    int chunkAmountZ_ = 0;
    std::size_t nextRebuild_ = 0;
    bool chunksInitialized_ = false;

    C3D_RenderTarget* target_ = nullptr;
    DVLB_s* shaderDvlb_ = nullptr;
    shaderProgram_s program_{};
    C3D_Mtx projection_{};
    int projectionLocation_ = -1;
    int modelViewLocation_ = -1;
    bool initialized_ = false;
};

} // namespace luckee
