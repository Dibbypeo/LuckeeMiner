#include "luckee/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <limits>
#include <vector>

#include "luckee/level.hpp"
#include "luckee/particle.hpp"
#include "luckee/player.hpp"
#include "luckee/tile.hpp"
#include "luckee/texture_loader.hpp"
#include "vshader_shbin.h"

namespace luckee {
namespace {

constexpr u32 CLEAR_COLOR = 0x80CCFFFF;

constexpr u32 DISPLAY_TRANSFER_FLAGS =
    GX_TRANSFER_FLIP_VERT(0) |
    GX_TRANSFER_OUT_TILED(0) |
    GX_TRANSFER_RAW_COPY(0) |
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

constexpr int CHUNK_SIZE = 16;
constexpr float PI = 3.14159265358979323846f;
constexpr int LAYERS = 2;
constexpr std::size_t INITIAL_VERTEX_RESERVE = 4096;
constexpr const char* TERRAIN_TEXTURE_PATH =
    "assets/textures/terrain.png";
constexpr const char* CHARACTER_TEXTURE_PATH =
    "assets/textures/char.png";
constexpr int ZOMBIE_VERTEX_COUNT = 216;
constexpr float HIGHLIGHT_OFFSET = 0.002f;

// Bound cached chunk geometry so explored areas cannot exhaust the original
// 3DS linear heap. Four 16-block chunks is a conservative render distance.
constexpr float RENDER_DISTANCE = 64.0f;
constexpr float MESH_CACHE_DISTANCE = 80.0f;

struct Face {
    float p[4][3];
    int nx;
    int ny;
    int nz;
    float shade;
};

constexpr Face faces[6] = {
    {{{0,0,1},{0,0,0},{1,0,0},{1,0,1}}, 0,-1,0, 1.0f},
    {{{1,1,1},{1,1,0},{0,1,0},{0,1,1}}, 0,1,0, 1.0f},
    {{{0,1,0},{1,1,0},{1,0,0},{0,0,0}}, 0,0,-1, 0.8f},
    {{{0,1,1},{0,0,1},{1,0,1},{1,1,1}}, 0,0,1, 0.8f},
    {{{0,1,1},{0,1,0},{0,0,0},{0,0,1}}, -1,0,0, 0.6f},
    {{{1,0,1},{1,0,0},{1,1,0},{1,1,1}}, 1,0,0, 0.6f}
};

static void faceUvs(
    int face,
    float minU, float maxU,
    float minV, float maxV,
    float (&u)[4], float (&v)[4]) {

    switch (face) {
        case 0:
            u[0] = minU; u[1] = minU;
            u[2] = maxU; u[3] = maxU;
            v[0] = maxV; v[1] = minV;
            v[2] = minV; v[3] = maxV;
            break;

        case 1:
            u[0] = maxU; u[1] = maxU;
            u[2] = minU; u[3] = minU;
            v[0] = maxV; v[1] = minV;
            v[2] = minV; v[3] = maxV;
            break;

        case 2:
            u[0] = maxU; u[1] = minU;
            u[2] = minU; u[3] = maxU;
            v[0] = minV; v[1] = minV;
            v[2] = maxV; v[3] = maxV;
            break;

        case 3:
            u[0] = minU; u[1] = minU;
            u[2] = maxU; u[3] = maxU;
            v[0] = minV; v[1] = maxV;
            v[2] = maxV; v[3] = minV;
            break;

        case 4:
            u[0] = maxU; u[1] = minU;
            u[2] = minU; u[3] = maxU;
            v[0] = minV; v[1] = minV;
            v[2] = maxV; v[3] = maxV;
            break;

        case 5:
            u[0] = minU; u[1] = maxU;
            u[2] = maxU; u[3] = minU;
            v[0] = maxV; v[1] = maxV;
            v[2] = minV; v[3] = minV;
            break;
    }
}

static void cameraRay(
    float yawDegrees, float pitchDegrees,
    float screenX, float screenY,
    float screenWidth, float screenHeight,
    float& dx, float& dy, float& dz) {

    const float yaw = yawDegrees * PI / 180.0f;
    const float pitch = pitchDegrees * PI / 180.0f;
    const float tanHalfFov = std::tan(35.0f * PI / 180.0f);
    const float aspect = screenWidth / screenHeight;

    const float ndcX = (screenX / screenWidth) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (screenY / screenHeight) * 2.0f;

    const float cameraX = ndcX * tanHalfFov * aspect;
    const float cameraY = ndcY * tanHalfFov;
    const float cameraZ = -1.0f;

    const float sinPitch = std::sin(pitch);
    const float cosPitch = std::cos(pitch);

    const float afterPitchY = cameraY * cosPitch + cameraZ * sinPitch;
    const float afterPitchZ = -cameraY * sinPitch + cameraZ * cosPitch;

    const float sinYaw = std::sin(yaw);
    const float cosYaw = std::cos(yaw);

    dx = cameraX * cosYaw - afterPitchZ * sinYaw;
    dy = afterPitchY;
    dz = cameraX * sinYaw + afterPitchZ * cosYaw;
}

static void cameraForward(
    float yawDegrees, float pitchDegrees,
    float& x, float& y, float& z) {

    const float yaw = yawDegrees * PI / 180.0f;
    const float pitch = pitchDegrees * PI / 180.0f;

    x = std::sin(yaw) * std::cos(pitch);
    y = -std::sin(pitch);
    z = -std::cos(yaw) * std::cos(pitch);
}

static bool raycastSample(
    const Level& level,
    float ox, float oy, float oz,
    float dx, float dy, float dz,
    int minX, int minY, int minZ,
    int maxX, int maxY, int maxZ,
    HitResult& result,
    float& depth) {

    int x = static_cast<int>(std::floor(ox));
    int y = static_cast<int>(std::floor(oy));
    int z = static_cast<int>(std::floor(oz));

    const int stepX = dx > 0.0f ? 1 : (dx < 0.0f ? -1 : 0);
    const int stepY = dy > 0.0f ? 1 : (dy < 0.0f ? -1 : 0);
    const int stepZ = dz > 0.0f ? 1 : (dz < 0.0f ? -1 : 0);

    const float infinity = std::numeric_limits<float>::infinity();
    const float deltaX = stepX ? std::abs(1.0f / dx) : infinity;
    const float deltaY = stepY ? std::abs(1.0f / dy) : infinity;
    const float deltaZ = stepZ ? std::abs(1.0f / dz) : infinity;

    float maxXCross = stepX > 0
        ? (static_cast<float>(x + 1) - ox) / dx
        : stepX < 0 ? (static_cast<float>(x) - ox) / dx : infinity;

    float maxYCross = stepY > 0
        ? (static_cast<float>(y + 1) - oy) / dy
        : stepY < 0 ? (static_cast<float>(y) - oy) / dy : infinity;

    float maxZCross = stepZ > 0
        ? (static_cast<float>(z + 1) - oz) / dz
        : stepZ < 0 ? (static_cast<float>(z) - oz) / dz : infinity;

    int enteredFace = -1;
    float t = 0.0f;

    for (int step = 0; step < 32; ++step) {
        if (x < minX || y < minY || z < minZ ||
            x >= maxX || y >= maxY || z >= maxZ) {
            return false;
        }

        if (level.isTile(x, y, z)) {
            result.x = x;
            result.y = y;
            result.z = z;
            result.type = 0;
            result.face = enteredFace >= 0 ? enteredFace : 0;
            depth = t;
            return true;
        }

        if (maxXCross <= maxYCross && maxXCross <= maxZCross) {
            t = maxXCross;
            maxXCross += deltaX;
            x += stepX;
            enteredFace = stepX > 0 ? 4 : 5;
        } else if (maxYCross <= maxZCross) {
            t = maxYCross;
            maxYCross += deltaY;
            y += stepY;
            enteredFace = stepY > 0 ? 0 : 1;
        } else {
            t = maxZCross;
            maxZCross += deltaZ;
            z += stepZ;
            enteredFace = stepZ > 0 ? 2 : 3;
        }
    }

    return false;
}

} // namespace

bool Renderer::initialize() {
    if (initialized_)
        return true;

    error_.clear();

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        error_ = "Could not initialize Citro3D.";
        return false;
    }

    initialized_ = true;

    target_ = C3D_RenderTargetCreate(
        240,
        400,
        GPU_RB_RGBA8,
        GPU_RB_DEPTH24_STENCIL8);

    if (!target_) {
        error_ = "Could not create the 3DS render target.";
        shutdown();
        return false;
    }

    C3D_RenderTargetSetOutput(
        target_,
        GFX_TOP,
        GFX_LEFT,
        DISPLAY_TRANSFER_FLAGS);

    shaderDvlb_ = DVLB_ParseFile(
        (u32*)vshader_shbin,
        vshader_shbin_size);

    if (!shaderDvlb_) {
        error_ = "Could not load the vertex shader.";
        shutdown();
        return false;
    }

    shaderProgramInit(&program_);
    shaderProgramSetVsh(
        &program_,
        &shaderDvlb_->DVLE[0]);
    C3D_BindProgram(&program_);

    projectionLocation_ =
        shaderInstanceGetUniformLocation(
            program_.vertexShader,
            "projection");

    modelViewLocation_ =
        shaderInstanceGetUniformLocation(
            program_.vertexShader,
            "modelView");

    if (projectionLocation_ < 0 ||
        modelViewLocation_ < 0) {
        error_ = "Required shader uniforms are missing.";
        shutdown();
        return false;
    }

    C3D_AttrInfo* attrInfo =
        C3D_GetAttrInfo();

    AttrInfo_Init(attrInfo);

    AttrInfo_AddLoader(
        attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(
        attrInfo, 1, GPU_FLOAT, 4);
    AttrInfo_AddLoader(
        attrInfo, 2, GPU_FLOAT, 2);

    for (int layer = 0; layer < LAYERS; ++layer)
        buildVertices_[layer].reserve(
            INITIAL_VERTEX_RESERVE);

    Mtx_PerspTilt(
        &projection_,
        70.0f * PI / 180.0f,
        C3D_AspectRatioTop,
        0.05f,
        1000.0f,
        false);

    if (!TextureLoader::loadTerrain(
            TERRAIN_TEXTURE_PATH,
            terrainTexture_,
            error_)) {
        shutdown();
        return false;
    }

    terrainTextureLoaded_ = true;

    if (!TextureLoader::loadCharacter(
            CHARACTER_TEXTURE_PATH,
            characterTexture_,
            error_)) {
        shutdown();
        return false;
    }

    characterTextureLoaded_ = true;

    characterVertices_.reserve(
        100u * ZOMBIE_VERTEX_COUNT);

    characterVboCapacity_ =
        100u * ZOMBIE_VERTEX_COUNT;

    characterVbo_ =
        linearAlloc(
            characterVboCapacity_ * sizeof(Vertex));

    if (!characterVbo_) {
        error_ =
            "Could not allocate zombie character buffer.";
        shutdown();
        return false;
    }

    C3D_TexSetFilter(
        &terrainTexture_,
        GPU_NEAREST,
        GPU_NEAREST);

    C3D_TexBind(
        0,
        &terrainTexture_);

    C3D_TexEnv* env =
        C3D_GetTexEnv(0);

    C3D_TexEnvInit(env);

    C3D_TexEnvSrc(
        env,
        C3D_Both,
        GPU_TEXTURE0,
        GPU_PRIMARY_COLOR);

    C3D_TexEnvFunc(
        env,
        C3D_Both,
        GPU_MODULATE);

    FogLut_Exp(
        &fogLut_,
        0.2f,
        2.0f,
        0.05f,
        1000.0f);

    C3D_FogColor(0x0E0B0A);
    C3D_FogLutBind(&fogLut_);

    C3D_DepthTest(
        true,
        GPU_GREATER,
        GPU_WRITE_ALL);

    C3D_CullFace(
        GPU_CULL_BACK_CCW);

    // The reference enables alpha testing for the character and bush
    // textures. A 0.5 threshold reproduces its GL_ALPHA_TEST behavior.
    C3D_AlphaTest(
        true,
        GPU_GREATER,
        0x80);

    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    return true;
}

void Renderer::shutdown() {
    if (initialized_ && !frameActive_)
        C3D_FrameSync();

    for (ChunkMesh& chunk : chunks_)
        releaseChunkMesh(chunk);

    chunks_.clear();
    visible_.clear();

    if (characterVbo_) {
        linearFree(characterVbo_);
        characterVbo_ = nullptr;
    }

    characterVboCapacity_ = 0;
    characterVertexCount_ = 0;
    characterVertices_.clear();

    if (characterTextureLoaded_) {
        C3D_TexDelete(&characterTexture_);
        characterTextureLoaded_ = false;
    }

    chunkAmountX_ = 0;
    chunkAmountY_ = 0;
    chunkAmountZ_ = 0;
    chunksInitialized_ = false;

    if (terrainTextureLoaded_) {
        C3D_TexDelete(
            &terrainTexture_);
        terrainTextureLoaded_ = false;
    }

    if (shaderDvlb_) {
        shaderProgramFree(&program_);
        DVLB_Free(shaderDvlb_);
        shaderDvlb_ = nullptr;
    }

    if (target_) {
        C3D_RenderTargetDelete(
            target_);
        target_ = nullptr;
    }

    if (initialized_) {
        C3D_Fini();
        initialized_ = false;
    }

    level_ = nullptr;
}

void Renderer::initializeChunks(
    const Level& level) {

    const int newAmountX =
        (level.width() + CHUNK_SIZE - 1) /
        CHUNK_SIZE;

    const int newAmountY =
        (level.depth() + CHUNK_SIZE - 1) /
        CHUNK_SIZE;

    const int newAmountZ =
        (level.height() + CHUNK_SIZE - 1) /
        CHUNK_SIZE;

    if (chunksInitialized_ &&
        chunkAmountX_ == newAmountX &&
        chunkAmountY_ == newAmountY &&
        chunkAmountZ_ == newAmountZ) {
        return;
    }

    for (ChunkMesh& chunk : chunks_)
        releaseChunkMesh(chunk);

    chunkAmountX_ = newAmountX;
    chunkAmountY_ = newAmountY;
    chunkAmountZ_ = newAmountZ;

    chunks_.clear();

    chunks_.resize(
        static_cast<std::size_t>(chunkAmountX_) *
        static_cast<std::size_t>(chunkAmountY_) *
        static_cast<std::size_t>(chunkAmountZ_));

    for (int x = 0;
         x < chunkAmountX_;
         ++x) {

        for (int y = 0;
             y < chunkAmountY_;
             ++y) {

            for (int z = 0;
                 z < chunkAmountZ_;
                 ++z) {

                const std::size_t index =
                    (static_cast<std::size_t>(x) +
                     static_cast<std::size_t>(y) *
                         static_cast<std::size_t>(chunkAmountX_)) *
                     static_cast<std::size_t>(chunkAmountZ_) +
                     static_cast<std::size_t>(z);

                ChunkMesh& chunk =
                    chunks_[index];

                chunk.minX =
                    x * CHUNK_SIZE;
                chunk.minY =
                    y * CHUNK_SIZE;
                chunk.minZ =
                    z * CHUNK_SIZE;

                chunk.maxX =
                    std::min(
                        level.width(),
                        chunk.minX + CHUNK_SIZE);

                chunk.maxY =
                    std::min(
                        level.depth(),
                        chunk.minY + CHUNK_SIZE);

                chunk.maxZ =
                    std::min(
                        level.height(),
                        chunk.minZ + CHUNK_SIZE);

                chunk.bounds = AABB(
                    static_cast<float>(chunk.minX),
                    static_cast<float>(chunk.minY),
                    static_cast<float>(chunk.minZ),
                    static_cast<float>(chunk.maxX),
                    static_cast<float>(chunk.maxY),
                    static_cast<float>(chunk.maxZ));

                chunk.dirty = true;
            }
        }
    }

    visible_.assign(chunks_.size(), 0);
    chunksInitialized_ = true;
}

void Renderer::markDirtyRange(
    const Level& level,
    int x0, int y0, int z0,
    int x1, int y1, int z1) {

    if (!chunksInitialized_)
        return;

    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    z0 = std::max(z0, 0);

    x1 = std::min(
        x1,
        level.width() - 1);

    y1 = std::min(
        y1,
        level.depth() - 1);

    z1 = std::min(
        z1,
        level.height() - 1);

    if (x0 > x1 ||
        y0 > y1 ||
        z0 > z1) {
        return;
    }

    const int cx0 =
        x0 / CHUNK_SIZE;
    const int cy0 =
        y0 / CHUNK_SIZE;
    const int cz0 =
        z0 / CHUNK_SIZE;

    const int cx1 =
        x1 / CHUNK_SIZE;
    const int cy1 =
        y1 / CHUNK_SIZE;
    const int cz1 =
        z1 / CHUNK_SIZE;

    for (int x = cx0;
         x <= cx1;
         ++x) {

        for (int y = cy0;
             y <= cy1;
             ++y) {

            for (int z = cz0;
                 z <= cz1;
                 ++z) {

                const std::size_t index =
                    (static_cast<std::size_t>(x) +
                     static_cast<std::size_t>(y) *
                         static_cast<std::size_t>(chunkAmountX_)) *
                     static_cast<std::size_t>(chunkAmountZ_) +
                     static_cast<std::size_t>(z);

                chunks_[index].dirty = true;
            }
        }
    }
}

void Renderer::tileChanged(
    int x, int y, int z) {

    if (!level_)
        return;

    markDirtyRange(
        *level_,
        x - 1, y - 1, z - 1,
        x + 1, y + 1, z + 1);
}

void Renderer::lightColumnChanged(
    int x, int z, int y0, int y1) {

    if (!level_)
        return;

    markDirtyRange(
        *level_,
        x - 1, y0 - 1, z - 1,
        x + 1, y1 + 1, z + 1);
}

void Renderer::allChanged() {
    for (ChunkMesh& chunk : chunks_)
        chunk.dirty = true;
}

void Renderer::appendFace(
    std::vector<Vertex>& vertices,
    int x, int y, int z,
    int face,
    float brightness,
    int textureId) const {

    const Face& f =
        faces[face];

    const int indices[6] =
        {0, 1, 2, 0, 2, 3};

    const float minU =
        static_cast<float>(textureId % 16) / 16.0f;

    const float maxU =
        minU + 0.0624375f;

    const float minV =
        static_cast<float>(textureId / 16) / 16.0f;

    const float maxV =
        minV + 0.0624375f;

    float u[4]{};
    float v[4]{};

    faceUvs(
        face,
        minU, maxU,
        minV, maxV,
        u, v);

    for (int index : indices) {
        Vertex vertex{};

        vertex.x =
            x + f.p[index][0];
        vertex.y =
            y + f.p[index][1];
        vertex.z =
            z + f.p[index][2];

        vertex.r = brightness;
        vertex.g = brightness;
        vertex.b = brightness;
        vertex.a = 1.0f;

        vertex.u = u[index];
        vertex.v = v[index];

        vertices.push_back(vertex);
    }
}

void Renderer::appendBush(
    std::vector<Vertex>& vertices,
    int x, int y, int z,
    float brightness) const {

    constexpr int textureId = 15;
    const float minU =
        static_cast<float>(textureId % 16) / 16.0f;
    const float maxU =
        minU + 0.0624375f;
    const float minV =
        static_cast<float>(textureId / 16) / 16.0f;
    const float maxV =
        minV + 0.0624375f;

    constexpr int rotations = 2;

    for (int r = 0; r < rotations; ++r) {
        const float angle =
            static_cast<float>(r) *
            PI / static_cast<float>(rotations) +
            0.7853981633974483f;

        const float xa =
            std::sin(angle) * 0.5f;
        const float za =
            std::cos(angle) * 0.5f;

        const float x0 =
            static_cast<float>(x) + 0.5f - xa;
        const float x1 =
            static_cast<float>(x) + 0.5f + xa;
        const float y0 =
            static_cast<float>(y);
        const float y1 =
            y0 + 1.0f;
        const float z0 =
            static_cast<float>(z) + 0.5f - za;
        const float z1 =
            static_cast<float>(z) + 0.5f + za;

        const float positions[8][3] = {
            {x0, y1, z0},
            {x1, y1, z1},
            {x1, y0, z1},
            {x0, y0, z0},
            {x1, y1, z1},
            {x0, y1, z0},
            {x0, y0, z0},
            {x1, y0, z1}
        };

        const float uvs[4][2] = {
            {maxU, minV},
            {minU, minV},
            {minU, maxV},
            {maxU, maxV}
        };

        // Two crossed quads, each split into two triangles.
        const int first[6] = {0, 1, 2, 0, 2, 3};
        const int second[6] = {4, 5, 6, 4, 6, 7};

        for (int index : first) {
            const int uvIndex = index;
            Vertex vertex{};
            vertex.x = positions[index][0];
            vertex.y = positions[index][1];
            vertex.z = positions[index][2];
            vertex.r = brightness;
            vertex.g = brightness;
            vertex.b = brightness;
            vertex.a = 1.0f;
            vertex.u = uvs[uvIndex][0];
            vertex.v = uvs[uvIndex][1];
            particleVertices_.push_back(vertex);
        }

        // Reverse face winding for the back-facing side, matching the source.
        const int reverse[6] = {4, 5, 7, 4, 7, 6};
        for (int index : reverse) {
            const int corner =
                (index == 4 ? 0 :
                 index == 5 ? 1 :
                 index == 7 ? 2 : 3);

            Vertex vertex{};
            vertex.x = positions[index][0];
            vertex.y = positions[index][1];
            vertex.z = positions[index][2];
            vertex.r = brightness;
            vertex.g = brightness;
            vertex.b = brightness;
            vertex.a = 1.0f;
            vertex.u = uvs[corner][0];
            vertex.v = uvs[corner][1];
            vertices.push_back(vertex);
        }
    }
}

bool Renderer::rebuildChunk(
    const Level& level,
    ChunkMesh& chunk) {

    for (int layer = 0;
         layer < LAYERS;
         ++layer) {
        buildVertices_[layer].clear();
    }

    for (int x = chunk.minX;
         x < chunk.maxX;
         ++x) {

        for (int y = chunk.minY;
             y < chunk.maxY;
             ++y) {

            for (int z = chunk.minZ;
                 z < chunk.maxZ;
                 ++z) {

                const int tileId =
                    level.getTile(x, y, z);

                if (tileId <= 0 ||
                    tileId >= Tile::MAX_TILES) {
                    continue;
                }

                Tile* tile =
                    Tile::tiles[tileId];

                if (!tile)
                    continue;

                if (tile->isCrossPlant()) {
                    const int renderLayer =
                        level.isLit(x, y, z) ? 0 : 1;

                    appendBush(
                        buildVertices_[renderLayer],
                        x, y, z,
                        1.0f);
                    continue;
                }

                for (int face = 0;
                     face < 6;
                     ++face) {
                    const Face& f =
                        faces[face];

                    const int nx =
                        x + f.nx;
                    const int ny =
                        y + f.ny;
                    const int nz =
                        z + f.nz;

                    if (level.isSolidTile(
                            nx, ny, nz)) {
                        continue;
                    }

                    const bool lit =
                        level.isLit(
                            nx, ny, nz);

                    const int renderLayer =
                        lit ? 0 : 1;

                    const float brightness =
                        lit
                            ? f.shade
                            : f.shade * 0.8f;

                    appendFace(
                        buildVertices_[renderLayer],
                        x, y, z,
                        face,
                        brightness,
                        tile->getTexture(face));
                }
            }
        }
    }

    void* newVbo[LAYERS] =
        {nullptr, nullptr};

    for (int layer = 0;
         layer < LAYERS;
         ++layer) {

        const std::size_t bytes =
            buildVertices_[layer].size() *
            sizeof(Vertex);

        if (bytes == 0)
            continue;

        newVbo[layer] =
            linearAlloc(bytes);

        if (!newVbo[layer]) {
            for (int cleanupLayer = 0;
                 cleanupLayer < LAYERS;
                 ++cleanupLayer) {

                if (newVbo[cleanupLayer])
                    linearFree(
                        newVbo[cleanupLayer]);
            }

            // Do not leave stale geometry behind when the linear heap is
            // temporarily too full for a replacement mesh. Releasing the
            // old mesh lets the next frame retry with the memory reclaimed.
            releaseChunkMesh(chunk);
            chunk.dirty = true;
            return false;
        }

        std::memcpy(
            newVbo[layer],
            buildVertices_[layer].data(),
            bytes);
    }

    for (int layer = 0;
         layer < LAYERS;
         ++layer) {

        if (chunk.vbo[layer])
            linearFree(
                chunk.vbo[layer]);

        chunk.vbo[layer] =
            newVbo[layer];

        chunk.vertexCount[layer] =
            static_cast<int>(
                buildVertices_[layer].size());
    }

    chunk.dirty = false;
    return true;
}

void Renderer::drawChunk(
    const ChunkMesh& chunk,
    int layer) {

    if (!chunk.vbo[layer] ||
        chunk.vertexCount[layer] <= 0) {
        return;
    }

    C3D_BufInfo* bufInfo =
        C3D_GetBufInfo();

    BufInfo_Init(bufInfo);

    BufInfo_Add(
        bufInfo,
        chunk.vbo[layer],
        sizeof(Vertex),
        3,
        0x210);

    C3D_SetBufInfo(bufInfo);

    C3D_DrawArrays(
        GPU_TRIANGLES,
        0,
        static_cast<u32>(
            chunk.vertexCount[layer]));
}

void Renderer::releaseChunkMesh(
    ChunkMesh& chunk) {

    for (int layer = 0;
         layer < LAYERS;
         ++layer) {

        if (chunk.vbo[layer]) {
            linearFree(
                chunk.vbo[layer]);
            chunk.vbo[layer] = nullptr;
        }

        chunk.vertexCount[layer] = 0;
    }
}

bool Renderer::pick(
    const Level& level,
    const Player& player,
    float alpha) {

    hasHit_ = false;

    const float boxMinX = player.x() - 0.3f;
    const float boxMaxX = player.x() + 0.3f;
    const float boxMinY = player.y() - 1.62f;
    const float boxMaxY = player.y() + 0.18f;
    const float boxMinZ = player.z() - 0.3f;
    const float boxMaxZ = player.z() + 0.3f;

    const int minX = std::max(0, static_cast<int>(boxMinX - 3.0f));
    const int maxX = std::min(level.width(), static_cast<int>(boxMaxX + 4.0f));
    const int minY = std::max(0, static_cast<int>(boxMinY - 3.0f));
    const int maxY = std::min(level.depth(), static_cast<int>(boxMaxY + 4.0f));
    const int minZ = std::max(0, static_cast<int>(boxMinZ - 3.0f));
    const int maxZ = std::min(level.height(), static_cast<int>(boxMaxZ + 4.0f));

    constexpr float screenWidth = 400.0f;
    constexpr float screenHeight = 240.0f;
    constexpr float centerX = screenWidth * 0.5f;
    constexpr float centerY = screenHeight * 0.5f;

    float forwardX = 0.0f;
    float forwardY = 0.0f;
    float forwardZ = 0.0f;

    cameraForward(player.yRot(), player.xRot(),
                  forwardX, forwardY, forwardZ);

    const float ox = player.renderX(alpha) - forwardX * 0.3f;
    const float oy = player.renderY(alpha) - forwardY * 0.3f;
    const float oz = player.renderZ(alpha) - forwardZ * 0.3f;

    float bestDepth = std::numeric_limits<float>::infinity();
    HitResult bestHit{};

    for (int sampleY = -2; sampleY <= 2; ++sampleY) {
        for (int sampleX = -2; sampleX <= 2; ++sampleX) {
            float dx = 0.0f;
            float dy = 0.0f;
            float dz = 0.0f;

            cameraRay(player.yRot(), player.xRot(),
                      centerX + static_cast<float>(sampleX),
                      centerY + static_cast<float>(sampleY),
                      screenWidth, screenHeight,
                      dx, dy, dz);

            float sampleDepth = 0.0f;
            HitResult sampleHit{};

            if (!raycastSample(
                    level, ox, oy, oz,
                    dx, dy, dz,
                    minX, minY, minZ,
                    maxX, maxY, maxZ,
                    sampleHit, sampleDepth)) {
                continue;
            }

            if (!hasHit_ || sampleDepth < bestDepth) {
                bestDepth = sampleDepth;
                bestHit = sampleHit;
                hasHit_ = true;
            }
        }
    }

    if (hasHit_)
        hitResult_ = bestHit;

    return hasHit_;
}

void Renderer::appendCharacterCube(
    std::vector<Vertex>& vertices,
    const Zombie& zombie,
    const CharacterPart& part,
    double time,
    float alpha) const {

    constexpr float size = 0.058333334f;
    const float yy =
        static_cast<float>(
            -std::abs(std::sin(time * 0.6662)) * 5.0 - 23.0);

    const float x0 = part.minX;
    const float y0 = part.minY;
    const float z0 = part.minZ;
    const float x1 = x0 + static_cast<float>(part.width);
    const float y1 = y0 + static_cast<float>(part.height);
    const float z1 = z0 + static_cast<float>(part.depth);

    const float verts[8][3] = {
        {x0, y0, z0},
        {x1, y0, z0},
        {x1, y1, z0},
        {x0, y1, z0},
        {x0, y0, z1},
        {x1, y0, z1},
        {x1, y1, z1},
        {x0, y1, z1}
    };

    struct Quad {
        int i0, i1, i2, i3;
        int u0, v0, u1, v1;
    };

    const float w = static_cast<float>(part.width);
    const float h = static_cast<float>(part.height);
    const float d = static_cast<float>(part.depth);
    const int tx = part.texX;
    const int ty = part.texY;

    const Quad quads[6] = {
        {5, 1, 2, 6,
         static_cast<int>(tx + d + w),
         static_cast<int>(ty + d),
         static_cast<int>(tx + d + w + d),
         static_cast<int>(ty + d + h)},
        {0, 4, 7, 3,
         tx,
         static_cast<int>(ty + d),
         static_cast<int>(tx + d),
         static_cast<int>(ty + d + h)},
        {5, 4, 0, 1,
         static_cast<int>(tx + d),
         ty,
         static_cast<int>(tx + d + w),
         static_cast<int>(ty + d)},
        {2, 3, 7, 6,
         static_cast<int>(tx + d + w),
         ty,
         static_cast<int>(tx + d + w + w),
         static_cast<int>(ty + d)},
        {1, 0, 3, 2,
         static_cast<int>(tx + d),
         static_cast<int>(ty + d),
         static_cast<int>(tx + d + w),
         static_cast<int>(ty + d + h)},
        {4, 5, 6, 7,
         static_cast<int>(tx + d + w + d),
         static_cast<int>(ty + d),
         static_cast<int>(tx + d + w + d + w),
         static_cast<int>(ty + d + h)}
    };

    auto transform = [&](const float p[3], float& outX, float& outY, float& outZ) {
        float x = p[0];
        float y = p[1];
        float z = p[2];

        // Cube.render(): rotate Z, then Y, then X in the OpenGL matrix stack.
        const float sx = std::sin(part.xRot);
        const float cx = std::cos(part.xRot);
        float ny = y * cx - z * sx;
        float nz = y * sx + z * cx;
        y = ny;
        z = nz;

        const float sy = std::sin(part.yRot);
        const float cy = std::cos(part.yRot);
        float nx = x * cy - z * sy;
        nz = x * sy + z * cy;
        x = nx;
        z = nz;

        const float sz = std::sin(part.zRot);
        const float cz = std::cos(part.zRot);
        nx = x * cz - y * sz;
        ny = x * sz + y * cz;
        x = nx;
        y = ny;

        x += part.x;
        y += part.y;
        z += part.z;

        // Zombie.render() rotates the complete model by rot + 180 degrees.
        const float bodyYaw = zombie.bodyRotation() + PI;
        const float sb = std::sin(bodyYaw);
        const float cb = std::cos(bodyYaw);
        nx = x * cb - z * sb;
        nz = x * sb + z * cb;
        x = nx;
        z = nz;

        // Match glScalef(1,-1,1), glScalef(size,size,size),
        // then glTranslatef(0, yy, 0) in the historical matrix order.
        x *= size;
        y = (y + yy) * -size;
        z *= size;

        x += zombie.renderX(alpha);
        y += zombie.renderY(alpha);
        z += zombie.renderZ(alpha);

        outX = x;
        outY = y;
        outZ = z;
    };

    const int triOrder[6] = {3, 2, 1, 3, 1, 0};

    for (const Quad& quad : quads) {
        const int ids[4] = {
            quad.i0, quad.i1, quad.i2, quad.i3
        };

        // Polygon's constructor remaps UVs to:
        // (u1,v0), (u0,v0), (u0,v1), (u1,v1).
        const float uv[4][2] = {
            {
                static_cast<float>(quad.u1) / 64.0f,
                static_cast<float>(quad.v0) / 32.0f
            },
            {
                static_cast<float>(quad.u0) / 64.0f,
                static_cast<float>(quad.v0) / 32.0f
            },
            {
                static_cast<float>(quad.u0) / 64.0f,
                static_cast<float>(quad.v1) / 32.0f
            },
            {
                static_cast<float>(quad.u1) / 64.0f,
                static_cast<float>(quad.v1) / 32.0f
            }
        };

        for (int corner : triOrder) {
            float px = 0.0f;
            float py = 0.0f;
            float pz = 0.0f;

            transform(
                verts[ids[corner]],
                px,
                py,
                pz);

            Vertex vertex{};
            vertex.x = px;
            vertex.y = py;
            vertex.z = pz;
            vertex.r = brightness;
            vertex.g = brightness;
            vertex.b = brightness;
            vertex.a = 1.0f;
            vertex.u = uv[corner][0];
            vertex.v = uv[corner][1];

            vertices.push_back(vertex);
        }
    }
}

void Renderer::renderZombies(
    const std::vector<Zombie>& zombies,
    const Player& player,
    float alpha,
    bool litLayer) {

    if (zombies.empty() ||
        !characterTextureLoaded_) {
        return;
    }

    characterVertices_.clear();

    const double now =
        std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

    const float renderX = player.renderX(alpha);
    const float renderY = player.renderY(alpha);
    const float renderZ = player.renderZ(alpha);

    constexpr float zombieRenderDistance = RENDER_DISTANCE;
    constexpr float zombieRenderDistanceSquared =
        zombieRenderDistance * zombieRenderDistance;

    for (const Zombie& zombie : zombies) {
        if (zombie.isLit() != litLayer)
            continue;

        const float dx = zombie.renderX(alpha) - renderX;
        const float dy = zombie.renderY(alpha) - renderY;
        const float dz = zombie.renderZ(alpha) - renderZ;

        if (dx * dx + dy * dy + dz * dz >
            zombieRenderDistanceSquared) {
            continue;
        }

        const AABB zombieBounds(
            zombie.renderX(alpha) - 1.0f,
            zombie.renderY(alpha) - 2.0f,
            zombie.renderZ(alpha) - 1.0f,
            zombie.renderX(alpha) + 1.0f,
            zombie.renderY(alpha) + 1.0f,
            zombie.renderZ(alpha) + 1.0f);

        if (!frustum_.cubeInFrustum(zombieBounds))
            continue;

        const double time =
            now * 10.0 *
            static_cast<double>(zombie.speed()) +
            static_cast<double>(zombie.timeOffset());

        CharacterPart parts[6] = {};
        const auto& baseParts = zombie.parts();

        for (int i = 0; i < 6; ++i)
            parts[i] = baseParts[static_cast<std::size_t>(i)];

        parts[0].yRot =
            static_cast<float>(std::sin(time * 0.83));
        parts[0].xRot =
            static_cast<float>(std::sin(time) * 0.8);

        parts[2].xRot =
            static_cast<float>(
                std::sin(time * 0.6662 + PI) * 2.0);
        parts[2].zRot =
            static_cast<float>(
                (std::sin(time * 0.2312) + 1.0) * 1.0);

        parts[3].xRot =
            static_cast<float>(
                std::sin(time * 0.6662) * 2.0);
        parts[3].zRot =
            static_cast<float>(
                (std::sin(time * 0.2812) - 1.0) * 1.0);

        parts[4].xRot =
            static_cast<float>(
                std::sin(time * 0.6662) * 1.4);

        parts[5].xRot =
            static_cast<float>(
                std::sin(time * 0.6662 + PI) * 1.4);

        for (const CharacterPart& part : parts)
            appendCharacterCube(
                characterVertices_,
                zombie,
                part,
                time,
                alpha,
                litLayer ? 1.0f : 0.6f);
    }

    const std::size_t required =
        characterVertices_.size();

    if (required == 0)
        return;

    if (required > characterVboCapacity_) {
        const std::size_t doubled =
            characterVboCapacity_ > 0
                ? characterVboCapacity_ * 2u
                : static_cast<std::size_t>(
                    ZOMBIE_VERTEX_COUNT);

        const std::size_t newCapacity =
            std::max(required, doubled);

        void* replacement =
            linearAlloc(
                newCapacity * sizeof(Vertex));

        if (!replacement)
            return;

        if (characterVbo_)
            linearFree(characterVbo_);

        characterVbo_ = replacement;
        characterVboCapacity_ = newCapacity;
    }

    std::memcpy(
        characterVbo_,
        characterVertices_.data(),
        required * sizeof(Vertex));

    characterVertexCount_ =
        static_cast<int>(required);

    C3D_TexBind(
        0,
        &characterTexture_);

    C3D_BufInfo* bufInfo =
        C3D_GetBufInfo();

    BufInfo_Init(bufInfo);
    BufInfo_Add(
        bufInfo,
        characterVbo_,
        sizeof(Vertex),
        3,
        0x210);
    C3D_SetBufInfo(bufInfo);

    C3D_DrawArrays(
        GPU_TRIANGLES,
        0,
        static_cast<u32>(
            characterVertexCount_));

    C3D_TexBind(
        0,
        &terrainTexture_);
}

void Renderer::renderParticles(
    const ParticleEngine& particleEngine,
    const Player& player,
    float alpha,
    bool litLayer) {

    const auto& particles =
        particleEngine.particles();

    if (particles.empty())
        return;

    particleVertices_.clear();
    particleVertices_.reserve(particles.size() * 6u);

    const float yaw =
        player.yRot() * PI / 180.0f;
    const float pitch =
        player.xRot() * PI / 180.0f;

    const float xa =
        -std::cos(yaw);
    const float za =
        -std::sin(yaw);
    const float xa2 =
        -za * std::sin(pitch);
    const float za2 =
        xa * std::sin(pitch);
    const float ya =
        std::cos(pitch);

    for (const std::unique_ptr<Particle>& holder :
         particles) {
        const Particle& particle = *holder;

        if (particle.isLit() != litLayer)
            continue;

        const int tex =
            particle.texture();

        const float u0 =
            (static_cast<float>(tex % 16) +
             particle.uOffset() / 4.0f) /
            16.0f;
        const float u1 =
            u0 + 0.015609375f;
        const float v0 =
            (static_cast<float>(tex / 16) +
             particle.vOffset() / 4.0f) /
            16.0f;
        const float v1 =
            v0 + 0.015609375f;

        const float r =
            0.1f * particle.size();

        const float x =
            particle.renderX(alpha);
        const float y =
            particle.renderY(alpha);
        const float z =
            particle.renderZ(alpha);

        const float pos[4][3] = {
            {
                x - xa * r - xa2 * r,
                y - ya * r,
                z - za * r - za2 * r
            },
            {
                x - xa * r + xa2 * r,
                y + ya * r,
                z - za * r + za2 * r
            },
            {
                x + xa * r + xa2 * r,
                y + ya * r,
                z + za * r + za2 * r
            },
            {
                x + xa * r - xa2 * r,
                y - ya * r,
                z + za * r - za2 * r
            }
        };

        const float uv[4][2] = {
            {u0, v1},
            {u0, v0},
            {u1, v0},
            {u1, v1}
        };

        const int index[6] =
            {0, 1, 2, 0, 2, 3};

        for (int i : index) {
            Vertex vertex{};
            vertex.x = pos[i][0];
            vertex.y = pos[i][1];
            vertex.z = pos[i][2];
            const float brightness =
                litLayer ? 0.8f : 0.48f;
            vertex.r = brightness;
            vertex.g = brightness;
            vertex.b = brightness;
            vertex.a = 1.0f;
            vertex.u = uv[i][0];
            vertex.v = uv[i][1];
            particleVertices_.push_back(vertex);
        }
    }

    if (particleVertices_.empty())
        return;

    const std::size_t required =
        particleVertices_.size();

    if (required > characterVboCapacity_) {
        const std::size_t doubled =
            characterVboCapacity_ > 0
                ? characterVboCapacity_ * 2u
                : required;

        const std::size_t newCapacity =
            std::max(required, doubled);

        void* replacement =
            linearAlloc(
                newCapacity * sizeof(Vertex));

        if (!replacement)
            return;

        if (characterVbo_)
            linearFree(characterVbo_);

        characterVbo_ = replacement;
        characterVboCapacity_ = newCapacity;
    }

    std::memcpy(
        characterVbo_,
        particleVertices_.data(),
        required * sizeof(Vertex));

    C3D_TexBind(
        0,
        &terrainTexture_);

    C3D_BufInfo* bufInfo =
        C3D_GetBufInfo();

    BufInfo_Init(bufInfo);
    BufInfo_Add(
        bufInfo,
        characterVbo_,
        sizeof(Vertex),
        3,
        0x210);
    C3D_SetBufInfo(bufInfo);

    C3D_DrawArrays(
        GPU_TRIANGLES,
        0,
        static_cast<u32>(required));
}

void Renderer::renderHit() {
    if (!hasHit_)
        return;

    const Face& face = faces[hitResult_.face];

    const auto now = std::chrono::system_clock::now();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

    const float pulse =
        std::sin(static_cast<double>(milliseconds) / 100.0) * 0.2f +
        0.4f;

    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE,
        GPU_SRC_ALPHA, GPU_ONE);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    const C3D_TexEnv savedEnv = *env;

    // RubyDung disables texturing while drawing the hit face. On Citro3D,
    // use the TEV stage as the equivalent of an untextured primary-color pass.
    C3D_TexEnvSrc(
        env,
        C3D_Both,
        GPU_PRIMARY_COLOR,
        GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(
        env,
        C3D_Both,
        GPU_REPLACE);

    C3D_ImmDrawBegin(GPU_TRIANGLES);

    const int indices[6] = {0, 1, 2, 0, 2, 3};

    for (int index : indices) {
        const float x =
            hitResult_.x + face.p[index][0] +
            face.nx * HIGHLIGHT_OFFSET;
        const float y =
            hitResult_.y + face.p[index][1] +
            face.ny * HIGHLIGHT_OFFSET;
        const float z =
            hitResult_.z + face.p[index][2] +
            face.nz * HIGHLIGHT_OFFSET;

        C3D_ImmSendAttrib(x, y, z, 1.0f);
        C3D_ImmSendAttrib(1.0f, 1.0f, 1.0f, pulse);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    }

    C3D_ImmDrawEnd();

    // Restore the exact terrain TEV state. C3D_SetTexEnv() marks the
    // stage dirty, ensuring the restored state is emitted to the GPU.
    C3D_SetTexEnv(
        0,
        const_cast<C3D_TexEnv*>(&savedEnv));

    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_ONE, GPU_ZERO,
        GPU_ONE, GPU_ZERO);

    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
}

void Renderer::render(
    const Level& level,
    const Player& player,
    const std::vector<Zombie>& zombies,
    const ParticleEngine& particleEngine,
    float alpha) {

    if (!initialized_ ||
        !terrainTextureLoaded_) {
        return;
    }

    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
        return;

    frameActive_ = true;

    initializeChunks(level);

    // FrameBegin(SYNCDRAW) above ensures previous GPU work is complete
    // before old linear-memory VBOs are released below.
    const float renderX =
        player.renderX(alpha);
    const float renderY =
        player.renderY(alpha);
    const float renderZ =
        player.renderZ(alpha);

    const float meshCacheDistanceSquared =
        MESH_CACHE_DISTANCE * MESH_CACHE_DISTANCE;

    for (ChunkMesh& chunk : chunks_) {
        const float centerX =
            (chunk.minX + chunk.maxX) * 0.5f;
        const float centerY =
            (chunk.minY + chunk.maxY) * 0.5f;
        const float centerZ =
            (chunk.minZ + chunk.maxZ) * 0.5f;

        const float dx = centerX - renderX;
        const float dy = centerY - renderY;
        const float dz = centerZ - renderZ;

        if (dx * dx + dy * dy + dz * dz >
            meshCacheDistanceSquared) {
            releaseChunkMesh(chunk);
            chunk.dirty = true;
        }
    }

    // Keep the renderer's world reference current.
    level_ = &level;

    C3D_Mtx modelView;
    Mtx_Identity(&modelView);

    // Match RubyDung.moveCameraToPlayer():
    // eye offset, player rotation, then interpolated player position.
    Mtx_Translate(
        &modelView,
        0.0f, 0.0f, -0.3f,
        true);

    Mtx_RotateX(
        &modelView,
        player.xRot() * PI / 180.0f,
        true);

    Mtx_RotateY(
        &modelView,
        player.yRot() * PI / 180.0f,
        true);

    Mtx_Translate(
        &modelView,
        -player.renderX(alpha),
        -player.renderY(alpha),
        -player.renderZ(alpha),
        true);

    C3D_Mtx clip;
    Mtx_Multiply(
        &clip,
        &projection_,
        &modelView);

    frustum_.set(clip);

    // The reference rebuilds a maximum of one chunk per rendered frame.
    // Prefer a visible dirty chunk closest to the player.
    std::size_t rebuildIndex =
        chunks_.size();

    float bestDistance =
        std::numeric_limits<float>::infinity();

    const float renderDistanceSquared =
        RENDER_DISTANCE * RENDER_DISTANCE;

    if (visible_.size() != chunks_.size())
        visible_.assign(chunks_.size(), 0);
    else
        std::fill(visible_.begin(), visible_.end(), 0);

    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {

        const ChunkMesh& chunk =
            chunks_[index];

        const float centerX =
            (chunk.minX + chunk.maxX) * 0.5f;
        const float centerY =
            (chunk.minY + chunk.maxY) * 0.5f;
        const float centerZ =
            (chunk.minZ + chunk.maxZ) * 0.5f;

        const float distanceX = centerX - renderX;
        const float distanceY = centerY - renderY;
        const float distanceZ = centerZ - renderZ;

        if (distanceX * distanceX +
            distanceY * distanceY +
            distanceZ * distanceZ >
            renderDistanceSquared) {
            continue;
        }

        if (!frustum_.cubeInFrustum(chunk.bounds))
            continue;

        visible_[index] = 1;

        if (!chunk.dirty)
            continue;

        const float distance =
            distanceX * distanceX +
            distanceZ * distanceZ;

        if (rebuildIndex ==
                chunks_.size() ||
            distance < bestDistance) {

            rebuildIndex = index;
            bestDistance = distance;
        }
    }

    if (rebuildIndex <
        chunks_.size()) {

        rebuildChunk(
            level,
            chunks_[rebuildIndex]);
    }

    C3D_RenderTargetClear(
        target_,
        C3D_CLEAR_ALL,
        CLEAR_COLOR,
        0);

    C3D_FrameDrawOn(target_);

    C3D_BindProgram(
        &program_);

    C3D_TexBind(
        0,
        &terrainTexture_);

    C3D_DepthTest(
        true,
        GPU_GREATER,
        GPU_WRITE_ALL);

    C3D_CullFace(
        GPU_CULL_BACK_CCW);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        projectionLocation_,
        &projection_);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        modelViewLocation_,
        &modelView);

    // Match the reference's two rendering passes: layer 0 is the bright,
    // unfogged pass; layer 1 is the darker, fogged pass.
    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {

        if (visible_[index])
            drawChunk(
                chunks_[index],
                0);
    }

    renderZombies(
        zombies,
        player,
        alpha,
        true);

    renderParticles(
        particleEngine,
        player,
        alpha,
        true);

    C3D_FogGasMode(
        GPU_FOG,
        GPU_PLAIN_DENSITY,
        false);

    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {

        if (visible_[index])
            drawChunk(
                chunks_[index],
                1);
    }

    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    renderHit();

    C3D_FrameEnd(0);
    frameActive_ = false;
}

} // namespace luckee
