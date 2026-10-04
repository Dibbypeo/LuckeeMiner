#include "luckee/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "luckee/level.hpp"
#include "luckee/player.hpp"
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

// Face order and coordinates are taken directly from rd-132211's Tile.render.
// The normal identifies the neighboring block used for culling and brightness.
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

} // namespace

bool Renderer::initialize() {
    if (initialized_) return true;
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;

    target_ = C3D_RenderTargetCreate(
        240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (!target_) {
        C3D_Fini();
        return false;
    }

    C3D_RenderTargetSetOutput(
        target_, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    shaderDvlb_ = DVLB_ParseFile(
        (u32*)vshader_shbin, vshader_shbin_size);
    if (!shaderDvlb_) {
        shutdown();
        return false;
    }

    shaderProgramInit(&program_);
    shaderProgramSetVsh(&program_, &shaderDvlb_->DVLE[0]);
    C3D_BindProgram(&program_);

    projectionLocation_ =
        shaderInstanceGetUniformLocation(program_.vertexShader, "projection");
    modelViewLocation_ =
        shaderInstanceGetUniformLocation(program_.vertexShader, "modelView");

    if (projectionLocation_ < 0 || modelViewLocation_ < 0) {
        shutdown();
        return false;
    }

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4);

    Mtx_PerspTilt(
        &projection_,
        70.0f * PI / 180.0f,
        C3D_AspectRatioTop,
        0.05f,
        1000.0f,
        false);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    for (ChunkMesh& chunk : chunks_) {
        for (int layer = 0; layer < LAYERS; ++layer) {
            if (chunk.vbo[layer]) {
                linearFree(chunk.vbo[layer]);
                chunk.vbo[layer] = nullptr;
            }
            chunk.vertexCount[layer] = 0;
        }
    }

    chunks_.clear();
    chunkAmountX_ = 0;
    chunkAmountY_ = 0;
    chunkAmountZ_ = 0;
    nextRebuild_ = 0;
    chunksInitialized_ = false;

    if (shaderDvlb_) {
        shaderProgramFree(&program_);
        DVLB_Free(shaderDvlb_);
        shaderDvlb_ = nullptr;
    }

    if (target_) {
        C3D_RenderTargetDelete(target_);
        target_ = nullptr;
    }

    if (initialized_) {
        C3D_Fini();
        initialized_ = false;
    }
}

void Renderer::initializeChunks(const Level& level) {
    const int newAmountX =
        (level.width() + CHUNK_SIZE - 1) / CHUNK_SIZE;
    const int newAmountY =
        (level.depth() + CHUNK_SIZE - 1) / CHUNK_SIZE;
    const int newAmountZ =
        (level.height() + CHUNK_SIZE - 1) / CHUNK_SIZE;

    if (chunksInitialized_ &&
        chunkAmountX_ == newAmountX &&
        chunkAmountY_ == newAmountY &&
        chunkAmountZ_ == newAmountZ) {
        return;
    }

    // This path is only expected during renderer startup or a dimension
    // change. No previous frame can reference these buffers during startup.
    for (ChunkMesh& chunk : chunks_) {
        for (int layer = 0; layer < LAYERS; ++layer) {
            if (chunk.vbo[layer]) {
                linearFree(chunk.vbo[layer]);
                chunk.vbo[layer] = nullptr;
            }
        }
    }

    chunkAmountX_ = newAmountX;
    chunkAmountY_ = newAmountY;
    chunkAmountZ_ = newAmountZ;

    chunks_.clear();
    chunks_.resize(
        static_cast<std::size_t>(chunkAmountX_) *
        static_cast<std::size_t>(chunkAmountY_) *
        static_cast<std::size_t>(chunkAmountZ_));

    for (int x = 0; x < chunkAmountX_; ++x) {
        for (int y = 0; y < chunkAmountY_; ++y) {
            for (int z = 0; z < chunkAmountZ_; ++z) {
                const std::size_t index =
                    (static_cast<std::size_t>(x) +
                     static_cast<std::size_t>(y) * chunkAmountX_) *
                    chunkAmountZ_ +
                    static_cast<std::size_t>(z);

                ChunkMesh& chunk = chunks_[index];
                chunk.minX = x * CHUNK_SIZE;
                chunk.minY = y * CHUNK_SIZE;
                chunk.minZ = z * CHUNK_SIZE;
                chunk.maxX =
                    std::min(level.width(), chunk.minX + CHUNK_SIZE);
                chunk.maxY =
                    std::min(level.depth(), chunk.minY + CHUNK_SIZE);
                chunk.maxZ =
                    std::min(level.height(), chunk.minZ + CHUNK_SIZE);
                chunk.dirty = true;
            }
        }
    }

    nextRebuild_ = 0;
    chunksInitialized_ = true;
}

void Renderer::appendFace(
    std::vector<Vertex>& vertices,
    int x, int y, int z,
    int face,
    float brightness) const {
    const Face& f = faces[face];
    const int indices[6] = {0, 1, 2, 0, 2, 3};

    for (int index : indices) {
        Vertex vertex{};
        vertex.x = x + f.p[index][0];
        vertex.y = y + f.p[index][1];
        vertex.z = z + f.p[index][2];
        vertex.r = brightness;
        vertex.g = brightness;
        vertex.b = brightness;
        vertex.a = 1.0f;
        vertices.push_back(vertex);
    }
}

bool Renderer::rebuildChunk(const Level& level, ChunkMesh& chunk) {
    std::vector<Vertex> layerVertices[LAYERS];

    for (int layer = 0; layer < LAYERS; ++layer)
        layerVertices[layer].reserve(INITIAL_VERTEX_RESERVE);

    for (int x = chunk.minX; x < chunk.maxX; ++x) {
        for (int y = chunk.minY; y < chunk.maxY; ++y) {
            for (int z = chunk.minZ; z < chunk.maxZ; ++z) {
                if (!level.isTile(x, y, z)) continue;

                for (int face = 0; face < 6; ++face) {
                    const Face& f = faces[face];

                    if (level.isSolidTile(
                            x + f.nx, y + f.ny, z + f.nz)) {
                        continue;
                    }

                    const float brightness =
                        level.getBrightness(
                            x + f.nx, y + f.ny, z + f.nz) * f.shade;

                    // rd-132211 includes the face in layer 0 when the
                    // brightness is the face's full shade, and layer 1
                    // otherwise.
                    const int layer =
                        (brightness == f.shade) ? 0 : 1;

                    appendFace(
                        layerVertices[layer],
                        x, y, z, face, brightness);
                }
            }
        }
    }

    // Upload both cached layer meshes. The caller runs this after
    // C3D_FrameBegin, so the previous frame's GPU work has completed.
    void* newVbo[LAYERS] = {nullptr, nullptr};

    for (int layer = 0; layer < LAYERS; ++layer) {
        const std::size_t bytes =
            layerVertices[layer].size() * sizeof(Vertex);

        if (bytes == 0) continue;

        newVbo[layer] = linearAlloc(bytes);
        if (!newVbo[layer]) {
            for (int cleanupLayer = 0; cleanupLayer < LAYERS; ++cleanupLayer) {
                if (newVbo[cleanupLayer])
                    linearFree(newVbo[cleanupLayer]);
            }
            return false;
        }

        std::memcpy(
            newVbo[layer],
            layerVertices[layer].data(),
            bytes);
    }

    for (int layer = 0; layer < LAYERS; ++layer) {
        if (chunk.vbo[layer])
            linearFree(chunk.vbo[layer]);

        chunk.vbo[layer] = newVbo[layer];
        chunk.vertexCount[layer] =
            static_cast<int>(layerVertices[layer].size());
    }

    chunk.dirty = false;
    return true;
}

void Renderer::drawChunk(const ChunkMesh& chunk, int layer) {
    if (!chunk.vbo[layer] || chunk.vertexCount[layer] <= 0) return;

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(
        bufInfo,
        chunk.vbo[layer],
        sizeof(Vertex),
        2,
        0x10);
    C3D_SetBufInfo(bufInfo);

    C3D_DrawArrays(
        GPU_TRIANGLES,
        0,
        static_cast<u32>(chunk.vertexCount[layer]));
}

void Renderer::render(
    const Level& level, const Player& player) {
    if (!initialized_) return;

    initializeChunks(level);

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_RenderTargetClear(target_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(target_);
    C3D_BindProgram(&program_);

    // Rebuild at most one dirty chunk per frame, matching the effective
    // rd-132211 rebuild limit while adapting the display lists to VBOs.
    std::size_t rebuildIndex = chunks_.size();
    float bestDistance = 0.0f;

    for (std::size_t index = 0; index < chunks_.size(); ++index) {
        const ChunkMesh& chunk = chunks_[index];
        if (!chunk.dirty) continue;

        const float centerX =
            (chunk.minX + chunk.maxX) * 0.5f;
        const float centerZ =
            (chunk.minZ + chunk.maxZ) * 0.5f;
        const float dx = centerX - player.x();
        const float dz = centerZ - player.z();
        const float distance = dx * dx + dz * dz;

        if (rebuildIndex == chunks_.size() || distance < bestDistance) {
            rebuildIndex = index;
            bestDistance = distance;
        }
    }

    if (rebuildIndex < chunks_.size()) {
        rebuildChunk(level, chunks_[rebuildIndex]);
        nextRebuild_ =
            (rebuildIndex + 1) % chunks_.size();
    }

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        projectionLocation_,
        &projection_);

    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_RotateX(
        &modelView,
        -player.xRot() * PI / 180.0f,
        true);
    Mtx_RotateY(
        &modelView,
        -player.yRot() * PI / 180.0f,
        true);
    Mtx_Translate(
        &modelView,
        -player.x(),
        -player.y(),
        -player.z(),
        true);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        modelViewLocation_,
        &modelView);

    for (int layer = 0; layer < LAYERS; ++layer) {
        for (const ChunkMesh& chunk : chunks_) {
            drawChunk(chunk, layer);
        }
    }

    C3D_FrameEnd(0);
}
 
} // namespace luckee
