#include "luckee/renderer.hpp"

#include <algorithm>
#include <cmath>

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
constexpr int CHUNK_RADIUS = 4;
constexpr float PI = 3.14159265358979323846f;

struct Face {
    float p[4][3];
    int nx, ny, nz;
    float shade;
};

constexpr Face faces[6] = {
    {{{0,0,1},{1,0,1},{1,0,0},{0,0,0}}, 0,-1,0, 0.6f},
    {{{0,1,0},{1,1,0},{1,1,1},{0,1,1}}, 0,1,0, 1.0f},
    {{{0,0,0},{1,0,0},{1,1,0},{0,1,0}}, 0,0,-1, 0.8f},
    {{{1,0,1},{0,0,1},{0,1,1},{1,1,1}}, 0,0,1, 0.8f},
    {{{0,0,1},{0,0,0},{0,1,0},{0,1,1}}, -1,0,0, 0.6f},
    {{{1,0,0},{1,0,1},{1,1,1},{1,1,0}}, 1,0,0, 0.6f}
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

    Mtx_PerspTilt(&projection_, 70.0f * PI / 180.0f,
                  C3D_AspectRatioTop, 0.05f, 1000.0f, false);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
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

void Renderer::sendFace(int x, int y, int z, int face, float brightness) {
    const Face& f = faces[face];

    for (int i : {0, 1, 2, 0, 2, 3}) {
        C3D_ImmSendAttrib(
            x + f.p[i][0], y + f.p[i][1], z + f.p[i][2], 1.0f);
        C3D_ImmSendAttrib(brightness, brightness, brightness, 1.0f);
    }
}

void Renderer::renderBlock(
    const Level& level, int x, int y, int z, int layer) {
    if (!level.isTile(x, y, z)) return;

    for (int face = 0; face < 6; ++face) {
        const Face& f = faces[face];

        if (level.isSolidTile(
                x + f.nx, y + f.ny, z + f.nz)) {
            continue;
        }

        const float brightness =
            f.shade * level.getBrightness(
                x + f.nx, y + f.ny, z + f.nz);

        const int expectedLayer = brightness >= 0.8f ? 1 : 0;
        if (expectedLayer != layer) continue;

        sendFace(x, y, z, face, brightness);
    }
}

void Renderer::renderLayer(
    const Level& level, int layer, const Player& player) {
    const int centerX =
        static_cast<int>(std::floor(player.x() / CHUNK_SIZE));
    const int centerZ =
        static_cast<int>(std::floor(player.z() / CHUNK_SIZE));

    C3D_ImmDrawBegin(GPU_TRIANGLES);

    for (int cx = centerX - CHUNK_RADIUS;
         cx <= centerX + CHUNK_RADIUS; ++cx) {
        for (int cz = centerZ - CHUNK_RADIUS;
             cz <= centerZ + CHUNK_RADIUS; ++cz) {
            const int x0 = std::max(0, cx * CHUNK_SIZE);
            const int z0 = std::max(0, cz * CHUNK_SIZE);
            const int x1 = std::min(level.width(), x0 + CHUNK_SIZE);
            const int z1 = std::min(level.height(), z0 + CHUNK_SIZE);

            if (x0 >= x1 || z0 >= z1) continue;

            for (int x = x0; x < x1; ++x)
                for (int y = 0; y < level.depth(); ++y)
                    for (int z = z0; z < z1; ++z)
                        renderBlock(level, x, y, z, layer);
        }
    }

    C3D_ImmDrawEnd();
}

void Renderer::render(
    const Level& level, const Player& player) {
    if (!initialized_) return;

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_RenderTargetClear(target_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(target_);
    C3D_BindProgram(&program_);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER, projectionLocation_, &projection_);

    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_RotateX(&modelView, -player.xRot() * PI / 180.0f, true);
    Mtx_RotateY(&modelView, -player.yRot() * PI / 180.0f, true);
    Mtx_Translate(
        &modelView, -player.x(), -player.y(), -player.z(), true);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER, modelViewLocation_, &modelView);

    renderLayer(level, 0, player);
    renderLayer(level, 1, player);

    C3D_FrameEnd(0);
}

} // namespace luckee
