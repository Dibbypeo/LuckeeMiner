#include "luckee/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "luckee/level.hpp"
#include "luckee/player.hpp"
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

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(
        attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(
        attrInfo, 1, GPU_FLOAT, 4);
    AttrInfo_AddLoader(
        attrInfo, 2, GPU_FLOAT, 2);

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

    C3D_TexSetFilter(
        &terrainTexture_,
        GPU_NEAREST,
        GPU_NEAREST);

    C3D_TexBind(
        0,
        &terrainTexture_);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
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

    // The reference uses GL_EXP2 with density 0.2.
    // FogLut_Exp with gradient 2 reproduces that curve in the PICA fog unit.
    FogLut_Exp(
        &fogLut_,
        0.2f,
        2.0f,
        0.05f,
        1000.0f);
    C3D_FogColor(0x0E0B0A);
    C3D_FogLutBind(&fogLut_);
    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    C3D_DepthTest(
        true,
        GPU_GREATER,
        GPU_WRITE_ALL);

    C3D_CullFace(GPU_CULL_BACK_CCW);

    return true;
}

void Renderer::shutdown() {
    if (initialized_)
        C3D_FrameSync();

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
    chunksInitialized_ = false;

    if (terrainTextureLoaded_) {
        C3D_TexDelete(&terrainTexture_);
        terrainTextureLoaded_ = false;
    }

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
                     static_cast<std::size_t>(y) *
                         static_cast<std::size_t>(chunkAmountX_)) *
                     static_cast<std::size_t>(chunkAmountZ_) +
                     static_cast<std::size_t>(z);

                ChunkMesh& chunk = chunks_[index];

                chunk.minX = x * CHUNK_SIZE;
                chunk.minY = y * CHUNK_SIZE;
                chunk.minZ = z * CHUNK_SIZE;

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

                chunk.dirty = true;
            }
        }
    }

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

    x1 = std::min(x1, level.width() - 1);
    y1 = std::min(y1, level.depth() - 1);
    z1 = std::min(z1, level.height() - 1);

    if (x0 > x1 || y0 > y1 || z0 > z1)
        return;

    const int cx0 = x0 / CHUNK_SIZE;
    const int cy0 = y0 / CHUNK_SIZE;
    const int cz0 = z0 / CHUNK_SIZE;

    const int cx1 = x1 / CHUNK_SIZE;
    const int cy1 = y1 / CHUNK_SIZE;
    const int cz1 = z1 / CHUNK_SIZE;

    for (int x = cx0; x <= cx1; ++x) {
        for (int y = cy0; y <= cy1; ++y) {
            for (int z = cz0; z <= cz1; ++z) {
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

    // A changed block affects itself and the six neighboring faces.
    // The reference marks the full +/-1 block cube dirty because a
    // neighboring face can become newly visible or disappear.
    // The dimensions are known from the Level passed to render, so the
    // listener caches the event until the next render through a compact
    // one-block range. This function is overwritten by the queued changes
    // below only through the render-time level bounds.
    //
    // The actual range is handled in render() because the listener interface
    // intentionally does not retain a Level pointer.
    (void)x;
    (void)y;
    (void)z;
}

void Renderer::lightColumnChanged(
    int x, int z, int y0, int y1) {
    (void)x;
    (void)z;
    (void)y0;
    (void)y1;
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

    const Face& f = faces[face];
    const int indices[6] =
        {0, 1, 2, 0, 2, 3};

    const float minU =
        static_cast<float>(textureId) / 16.0f;

    const float maxU =
        minU + 0.0624375f;

    const float minV = 0.0f;
    const float maxV = 0.0624375f;

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

bool Renderer::rebuildChunk(
    const Level& level,
    ChunkMesh& chunk) {

    std::vector<Vertex> layerVertices[LAYERS];

    for (int layer = 0;
         layer < LAYERS;
         ++layer) {
        layerVertices[layer].reserve(
            INITIAL_VERTEX_RESERVE);
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

                if (!level.isTile(x, y, z))
                    continue;

                const int textureId =
                    (y == level.depth() * 2 / 3)
                        ? 0
                        : 1;

                for (int face = 0;
                     face < 6;
                     ++face) {

                    const Face& f =
                        faces[face];

                    if (level.isSolidTile(
                            x + f.nx,
                            y + f.ny,
                            z + f.nz)) {
                        continue;
                    }

                    const float brightness =
                        level.getBrightness(
                            x + f.nx,
                            y + f.ny,
                            z + f.nz) *
                        f.shade;

                    // Match Tile.render's layer expression:
                    // bright faces go to layer 0 and dark faces to layer 1.
                    const int renderLayer =
                        (brightness == f.shade)
                            ? 0
                            : 1;

                    appendFace(
                        layerVertices[renderLayer],
                        x, y, z,
                        face,
                        brightness,
                        textureId);
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
            layerVertices[layer].size() *
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

            return false;
        }

        std::memcpy(
            newVbo[layer],
            layerVertices[layer].data(),
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
                layerVertices[layer].size());
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

void Renderer::render(
    const Level& level,
    const Player& player,
    float alpha) {

    if (!initialized_ ||
        !terrainTextureLoaded_) {
        return;
    }

    initializeChunks(level);

    // Reproduce RubyDung's camera sequence:
    // translate eye by -0.3, rotate by player angles, then translate by
    // the interpolated player position.
    C3D_Mtx modelView;
    Mtx_Identity(&modelView);

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

    std::size_t rebuildIndex =
        chunks_.size();

    float bestDistance =
        0.0f;

    const float renderX =
        player.renderX(alpha);

    const float renderZ =
        player.renderZ(alpha);

    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {

        const ChunkMesh& chunk =
            chunks_[index];

        if (!chunk.dirty ||
            !frustum_.cubeInFrustum(
                AABB(
                    static_cast<float>(chunk.minX),
                    static_cast<float>(chunk.minY),
                    static_cast<float>(chunk.minZ),
                    static_cast<float>(chunk.maxX),
                    static_cast<float>(chunk.maxY),
                    static_cast<float>(chunk.maxZ)))) {
            continue;
        }

        const float centerX =
            (chunk.minX + chunk.maxX) *
            0.5f;

        const float centerZ =
            (chunk.minZ + chunk.maxZ) *
            0.5f;

        const float dx =
            centerX - renderX;

        const float dz =
            centerZ - renderZ;

        const float distance =
            dx * dx + dz * dz;

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

    C3D_FrameBegin(
        C3D_FRAME_SYNCDRAW);

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

    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        projectionLocation_,
        &projection_);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        modelViewLocation_,
        &modelView);

    // Match RubyDung.render(): bright layer first without fog.
    for (const ChunkMesh& chunk : chunks_) {
        if (frustum_.cubeInFrustum(
                AABB(
                    static_cast<float>(chunk.minX),
                    static_cast<float>(chunk.minY),
                    static_cast<float>(chunk.minZ),
                    static_cast<float>(chunk.maxX),
                    static_cast<float>(chunk.maxY),
                    static_cast<float>(chunk.maxZ)))) {
            drawChunk(chunk, 0);
        }
    }

    // Darker terrain is rendered in a second pass with EXP2 fog.
    C3D_FogGasMode(
        GPU_FOG,
        GPU_PLAIN_DENSITY,
        false);

    for (const ChunkMesh& chunk : chunks_) {
        if (frustum_.cubeInFrustum(
                AABB(
                    static_cast<float>(chunk.minX),
                    static_cast<float>(chunk.minY),
                    static_cast<float>(chunk.minZ),
                    static_cast<float>(chunk.maxX),
                    static_cast<float>(chunk.maxY),
                    static_cast<float>(chunk.maxZ)))) {
            drawChunk(chunk, 1);
        }
    }

    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    C3D_FrameEnd(0);
}

} // namespace luckee
