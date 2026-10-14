#include "luckee/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
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

// Both screens retain gfxInitDefault()'s BGR8 format during normal play.
 // Fatal-error consoles are initialized only after the renderer has shut down.
constexpr u32 TOP_DISPLAY_TRANSFER_FLAGS =
    GX_TRANSFER_FLIP_VERT(0) |
    GX_TRANSFER_OUT_TILED(0) |
    GX_TRANSFER_RAW_COPY(0) |
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

constexpr u32 BOTTOM_DISPLAY_TRANSFER_FLAGS =
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
constexpr const char* ROCK_TEXTURE_PATH =
    "assets/textures/rock.png";
constexpr const char* HORIZON_WATER_TEXTURE_PATH =
    "assets/textures/water.png";
constexpr const char* DIRT_TEXTURE_PATH =
    "assets/textures/dirt.png";
constexpr const char* FONT_TEXTURE_PATH =
    "assets/textures/default.gif";
constexpr int ZOMBIE_VERTEX_COUNT = 216;
constexpr float HIGHLIGHT_OFFSET = 0.002f;

// Bound cached chunk geometry so explored areas cannot exhaust the original
// 3DS linear heap. Four 16-block chunks is a conservative render distance.
constexpr float RENDER_DISTANCE = 64.0f;
constexpr float MESH_CACHE_DISTANCE = 80.0f;
constexpr int MAX_REBUILDS_PER_FRAME = 8;

static std::uint64_t currentTimeMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

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

    // The ray is limited by the caller's reference selection volume.
    for (int step = 0; step < 1024; ++step) {
        if (x < minX || y < minY || z < minZ ||
            x >= maxX || y >= maxY || z >= maxZ) {
            return false;
        }

        if (level.isSolidTile(x, y, z)) {
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


void Renderer::drawDirtBackground(float width, float height) {
    if (!dirtTextureLoaded_)
        return;

    C3D_BindProgram(&program_);
    Mtx_OrthoTilt(&projection_, 0.0f, width, height, 0.0f, 100.0f, 300.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, 0.0f, 0.0f, -200.0f, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, modelViewLocation_, &modelView);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    const C3D_TexEnv savedEnv = *env;
    C3D_TexBind(0, &dirtTexture_);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    const float points[4][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {width, 0.0f, width / 16.0f, 0.0f},
        {width, height, width / 16.0f, height / 16.0f},
        {0.0f, height, 0.0f, height / 16.0f}
    };
    constexpr int indices[6] = {0, 1, 2, 0, 2, 3};
    C3D_ImmDrawBegin(GPU_TRIANGLES);
    for (int i : indices) {
        C3D_ImmSendAttrib(points[i][0], points[i][1], 0.0f, 1.0f);
        C3D_ImmSendAttrib(1.0f, 1.0f, 1.0f, 1.0f);
        C3D_ImmSendAttrib(points[i][2], points[i][3], 0.0f, 0.0f);
    }
    C3D_ImmDrawEnd();

    C3D_SetTexEnv(0, const_cast<C3D_TexEnv*>(&savedEnv));
    C3D_TexBind(0, &terrainTexture_);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_AlphaTest(true, GPU_GREATER, 0x80);
}

void Renderer::drawBitmapText(
    const char* text, float x, float y, float width, float height,
    float red, float green, float blue, float scale) {
    if (!fontTextureLoaded_ || !text)
        return;

    // Bounds are part of the UI helper API; the caller provides layout room.
    // Bitmap strings are currently kept short enough to fit their target screen.
    (void)width;
    (void)height;

    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, 0.0f, 0.0f, -200.0f, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, modelViewLocation_, &modelView);

    C3D_BindProgram(&program_);
    C3D_TexBind(0, &fontTexture_);
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    const C3D_TexEnv savedEnv = *env;
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    constexpr int indices[6] = {0, 1, 2, 0, 2, 3};
    float cursor = x;
    C3D_ImmDrawBegin(GPU_TRIANGLES);
    for (const char* p = text; *p; ++p) {
        unsigned int code = static_cast<unsigned char>(*p);
        if (code >= 128u) code = static_cast<unsigned int>('?');
        const float u0 = static_cast<float>(code % 16u) / 16.0f;
        const float v0 = static_cast<float>(code / 16u) / 16.0f;
        const float u1 = u0 + 1.0f / 16.0f;
        const float v1 = v0 + 1.0f / 16.0f;
        const float x1 = cursor + 8.0f * scale;
        const float y1 = y + 8.0f * scale;
        const float points[4][4] = {
            {cursor, y, u0, v0},
            {x1, y, u1, v0},
            {x1, y1, u1, v1},
            {cursor, y1, u0, v1}
        };
        for (int i : indices) {
            C3D_ImmSendAttrib(points[i][0], points[i][1], 0.0f, 1.0f);
            C3D_ImmSendAttrib(red, green, blue, 1.0f);
            C3D_ImmSendAttrib(points[i][2], points[i][3], 0.0f, 0.0f);
        }
        cursor += (static_cast<float>(fontWidths_[code]) + 1.0f) * scale;
    }
    C3D_ImmDrawEnd();

    C3D_SetTexEnv(0, const_cast<C3D_TexEnv*>(&savedEnv));
    C3D_TexBind(0, &terrainTexture_);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_AlphaTest(true, GPU_GREATER, 0x80);
}

void Renderer::drawShadowedText(
    const char* text, float x, float y, float width, float height,
    float scale) {
    drawBitmapText(text, x + 1.0f, y + 1.0f, width, height, 0.0f, 0.0f, 0.0f, scale);
    drawBitmapText(text, x, y, width, height, 1.0f, 1.0f, 1.0f, scale);
}

void Renderer::drawSolidRect(
    float x, float y, float width, float height,
    float red, float green, float blue, float alpha) {
    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, 0.0f, 0.0f, -200.0f, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, modelViewLocation_, &modelView);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    const C3D_TexEnv savedEnv = *env;
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    const float p[4][2] = {
        {x, y}, {x + width, y},
        {x + width, y + height}, {x, y + height}
    };
    constexpr int indices[6] = {0, 1, 2, 0, 2, 3};
    C3D_ImmDrawBegin(GPU_TRIANGLES);
    for (int i : indices) {
        C3D_ImmSendAttrib(p[i][0], p[i][1], 0.0f, 1.0f);
        C3D_ImmSendAttrib(red, green, blue, alpha);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    }
    C3D_ImmDrawEnd();

    C3D_SetTexEnv(0, const_cast<C3D_TexEnv*>(&savedEnv));
    C3D_TexBind(0, &terrainTexture_);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_AlphaTest(true, GPU_GREATER, 0x80);
}

void Renderer::drawBottomMenu(
    const char* title, const char* const* entries, int count,
    int selected, const char* status) {
    drawDirtBackground(320.0f, 240.0f);

    const float titleX =
        (title && std::strcmp(title, "LUCKEEMINER") == 0)
            ? 128.0f
            : 20.0f;
    drawShadowedText(title ? title : "MENU", titleX, 16.0f, 320.0f, 240.0f, 1.0f);
    const int visibleCount = std::min(count, 6);
    const float firstY = visibleCount > 4 ? 50.0f : 66.0f;
    for (int i = 0; i < visibleCount; ++i) {
        const float y = firstY + static_cast<float>(i) * 24.0f;
        if (i == selected)
            drawSolidRect(18.0f, y - 4.0f, 284.0f, 19.0f, 0.05f, 0.05f, 0.05f, 0.62f);
        if (i == selected)
            drawShadowedText(">", 28.0f, y, 320.0f, 240.0f, 1.0f);
        drawShadowedText(entries && entries[i] ? entries[i] : "", 44.0f, y, 320.0f, 240.0f, 1.0f);
    }

    if (status && *status) {
        char line[42]{};
        std::size_t n = std::min<std::size_t>(std::strlen(status), sizeof(line) - 1);
        std::memcpy(line, status, n);
        drawShadowedText(line, 18.0f, 190.0f, 320.0f, 240.0f, 1.0f);
    }
    drawShadowedText("D-PAD: MOVE  A: SELECT", 20.0f, 218.0f, 320.0f, 240.0f, 1.0f);
}

void Renderer::drawGameplayBottom(
    int selectedTileId, bool placingMode, const Player& player) {
    drawDirtBackground(320.0f, 240.0f);
    drawShadowedText("LUCKEEMINER", 128.0f, 12.0f, 320.0f, 240.0f, 1.0f);
    drawShadowedText("WORLD ACTIVE", 18.0f, 35.0f, 320.0f, 240.0f, 1.0f);
    drawSolidRect(16.0f, 48.0f, 288.0f, 1.0f, 0.72f, 0.60f, 0.42f, 1.0f);

    const char* blockName = "UNKNOWN";
    switch (selectedTileId) {
        case 1: blockName = "STONE"; break;
        case 2: blockName = "GRASS"; break;
        case 3: blockName = "DIRT"; break;
        case 4: blockName = "COBBLESTONE"; break;
        case 5: blockName = "PLANKS"; break;
        case 6: blockName = "SAPLING"; break;
        case 7: blockName = "BEDROCK"; break;
        case 8: blockName = "FLOWING WATER"; break;
        case 9: blockName = "WATER"; break;
        case 10: blockName = "FLOWING LAVA"; break;
        case 11: blockName = "LAVA"; break;
        default: break;
    }

    char line[48]{};
    std::snprintf(line, sizeof(line), "BLOCK: %s", blockName);
    drawShadowedText(line, 18.0f, 62.0f, 320.0f, 240.0f, 1.0f);
    drawShadowedText(placingMode ? "MODE: PLACE" : "MODE: BREAK",
                     18.0f, 84.0f, 320.0f, 240.0f, 1.0f);

    std::snprintf(line, sizeof(line), "POS: %.1f %.1f %.1f",
                  player.x(), player.y(), player.z());
    drawShadowedText(line, 18.0f, 106.0f, 320.0f, 240.0f, 1.0f);

    drawSolidRect(16.0f, 128.0f, 288.0f, 1.0f, 0.72f, 0.60f, 0.42f, 1.0f);
    drawShadowedText("D-PAD: CHANGE BLOCK", 18.0f, 140.0f, 320.0f, 240.0f, 1.0f);
    drawShadowedText("L: ACTION   R: MODE", 18.0f, 162.0f, 320.0f, 240.0f, 1.0f);
    drawShadowedText("A: JUMP   START: PAUSE", 18.0f, 184.0f, 320.0f, 240.0f, 1.0f);
    drawShadowedText("TOUCH: LOOK", 18.0f, 206.0f, 320.0f, 240.0f, 1.0f);
}

void Renderer::renderMainMenu(int selected) {
    if (!initialized_ || !bottomTarget_ || !dirtTextureLoaded_ || !fontTextureLoaded_)
        return;
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
        return;
    frameActive_ = true;
    const C3D_Mtx savedProjection = projection_;

    C3D_RenderTargetClear(target_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(target_);
    drawDirtBackground(400.0f, 240.0f);
    drawShadowedText("MINECRAFT CLASSIC", 154.0f, 108.0f, 400.0f, 240.0f, 1.0f);

    static const char* const entries[] = {"Play", "Exit"};
    C3D_RenderTargetClear(bottomTarget_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(bottomTarget_);
    drawBottomMenu("LUCKEEMINER", entries, 2, selected, nullptr);

    projection_ = savedProjection;
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
    C3D_FrameEnd(0);
    frameActive_ = false;
}

void Renderer::renderLoadingScreen(const char* stage, int percent) {
    if (!initialized_ || !bottomTarget_ || !dirtTextureLoaded_ || !fontTextureLoaded_)
        return;
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
        return;
    frameActive_ = true;
    const C3D_Mtx savedProjection = projection_;

    C3D_RenderTargetClear(target_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(target_);
    drawDirtBackground(400.0f, 240.0f);
    drawShadowedText("MINECRAFT CLASSIC", 154.0f, 108.0f, 400.0f, 240.0f, 1.0f);

    C3D_RenderTargetClear(bottomTarget_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(bottomTarget_);
    drawDirtBackground(320.0f, 240.0f);
    drawShadowedText("GENERATING WORLD", 116.0f, 42.0f, 320.0f, 240.0f, 1.0f);
    drawShadowedText(stage ? stage : "Working", 20.0f, 78.0f, 320.0f, 240.0f, 1.0f);
    char percentText[16];
    std::snprintf(percentText, sizeof(percentText), "%d%%", std::max(0, std::min(100, percent)));
    drawShadowedText(percentText, 20.0f, 102.0f, 320.0f, 240.0f, 1.0f);
    drawSolidRect(18.0f, 132.0f, 284.0f, 16.0f, 0.05f, 0.05f, 0.05f, 0.88f);
    const int safePercent = std::max(0, std::min(100, percent));
    drawSolidRect(22.0f, 136.0f, 276.0f * safePercent / 100.0f, 8.0f,
                  0.78f, 0.78f, 0.78f, 1.0f);
    drawShadowedText("PLEASE WAIT", 130.0f, 168.0f, 320.0f, 240.0f, 1.0f);

    projection_ = savedProjection;
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
    C3D_FrameEnd(0);
    frameActive_ = false;
}


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
        TOP_DISPLAY_TRANSFER_FLAGS);

    bottomTarget_ = C3D_RenderTargetCreate(
        240,
        320,
        GPU_RB_RGBA8,
        GPU_RB_DEPTH24_STENCIL8);
    if (!bottomTarget_) {
        error_ = "Could not create the bottom-screen render target.";
        shutdown();
        return false;
    }
    C3D_RenderTargetSetOutput(
        bottomTarget_,
        GFX_BOTTOM,
        GFX_LEFT,
        BOTTOM_DISPLAY_TRANSFER_FLAGS);

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

    if (!TextureLoader::loadRock(
            ROCK_TEXTURE_PATH, rockTexture_, error_)) {
        shutdown();
        return false;
    }
    rockTextureLoaded_ = true;

    if (!TextureLoader::loadHorizonWater(
            HORIZON_WATER_TEXTURE_PATH, horizonWaterTexture_, error_)) {
        shutdown();
        return false;
    }
    horizonWaterTextureLoaded_ = true;

    if (!TextureLoader::loadDirt(
            DIRT_TEXTURE_PATH, dirtTexture_, error_)) {
        shutdown();
        return false;
    }
    dirtTextureLoaded_ = true;

    if (!TextureLoader::loadFontGif(
            FONT_TEXTURE_PATH, fontTexture_, fontWidths_, error_)) {
        shutdown();
        return false;
    }
    fontTextureLoaded_ = true;

    // The native Classic branch starts with no automatically-spawned zombies.
    // Grow the character buffers only if the player later spawns them.
    // additional zombies so the original 3DS does not reserve unused memory.
    characterVertices_.reserve(
        10u * ZOMBIE_VERTEX_COUNT);

    characterVboCapacity_ =
        10u * ZOMBIE_VERTEX_COUNT;

    characterVbo_ =
        linearAlloc(
            characterVboCapacity_ * sizeof(Vertex));

    if (!characterVbo_) {
        error_ =
            "Could not allocate zombie character buffer.";
        shutdown();
        return false;
    }

    particleVboCapacity_ = 4096;
    particleVbo_ =
        linearAlloc(
            particleVboCapacity_ * sizeof(Vertex));

    if (!particleVbo_) {
        error_ =
            "Could not allocate particle buffer.";
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

    // The extracted client uses GL_EXP fog with different densities
    // and colors for daylight and shadow passes.
    FogLut_Exp(
        &dayFogLut_,
        0.001f,
        1.0f,
        0.05f,
        1000.0f);

    FogLut_Exp(
        &shadowFogLut_,
        0.01f,
        1.0f,
        0.05f,
        1000.0f);

    FogLut_Exp(
        &waterFogLut_,
        0.1f,
        1.0f,
        0.05f,
        1000.0f);

    FogLut_Exp(
        &lavaFogLut_,
        2.0f,
        1.0f,
        0.05f,
        1000.0f);

    C3D_FogColor(0xFEFBFA);
    C3D_FogLutBind(&dayFogLut_);

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

    if (particleVbo_) {
        linearFree(particleVbo_);
        particleVbo_ = nullptr;
    }

    if (hudVbo_) {
        linearFree(hudVbo_);
        hudVbo_ = nullptr;
    }
    hudVboCapacity_ = 0;
    hudVertices_.clear();

    if (horizonRockVbo_) {
        linearFree(horizonRockVbo_);
        horizonRockVbo_ = nullptr;
    }
    if (horizonWaterVbo_) {
        linearFree(horizonWaterVbo_);
        horizonWaterVbo_ = nullptr;
    }
    horizonRockVertexCount_ = 0;
    horizonWaterVertexCount_ = 0;
    horizonWidth_ = 0;
    horizonLength_ = 0;

    particleVboCapacity_ = 0;
    particleVertices_.clear();

    if (characterTextureLoaded_) {
        C3D_TexDelete(&characterTexture_);
        characterTextureLoaded_ = false;
    }

    if (rockTextureLoaded_) {
        C3D_TexDelete(&rockTexture_);
        rockTextureLoaded_ = false;
    }

    if (horizonWaterTextureLoaded_) {
        C3D_TexDelete(&horizonWaterTexture_);
        horizonWaterTextureLoaded_ = false;
    }
    if (dirtTextureLoaded_) {
        C3D_TexDelete(&dirtTexture_);
        dirtTextureLoaded_ = false;
    }
    if (fontTextureLoaded_) {
        C3D_TexDelete(&fontTexture_);
        fontTextureLoaded_ = false;
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

    if (bottomTarget_) {
        C3D_RenderTargetDelete(bottomTarget_);
        bottomTarget_ = nullptr;
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

                ChunkMesh& chunk = chunks_[index];
                if (!chunk.dirty)
                    chunk.dirtiedTimeMs = currentTimeMs();
                chunk.dirty = true;
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
    if (!level_)
        return;

    markDirtyRange(
        *level_,
        0, 0, 0,
        level_->width() - 1,
        level_->depth() - 1,
        level_->height() - 1);
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
        const double angle =
            static_cast<double>(r) *
            3.14159265358979323846 /
            static_cast<double>(rotations) +
            0.7853981633974483;

        const float xa =
            static_cast<float>(
                std::sin(angle) * 0.5);
        const float za =
            static_cast<float>(
                std::cos(angle) * 0.5);

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
            vertices.push_back(vertex);
        }

        // The extracted source emits the second side in reverse order so
        // both sides remain visible with back-face culling enabled.
        const int reverse[6] = {4, 5, 6, 4, 6, 7};
        for (int index : reverse) {
            const int corner =
                (index == 4 ? 0 :
                 index == 5 ? 1 :
                 index == 6 ? 2 : 3);

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

void Renderer::appendFluidFace(
    std::vector<Vertex>& vertices,
    int x, int y, int z,
    int face,
    float brightness,
    int textureId) const {

    const Face& f = faces[face];
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    const float minU = static_cast<float>(textureId % 16) / 16.0f;
    const float maxU = minU + 0.0624375f;
    const float minV = static_cast<float>(textureId / 16) / 16.0f;
    const float maxV = minV + 0.0624375f;
    float u[4]{};
    float v[4]{};
    faceUvs(face, minU, maxU, minV, maxV, u, v);

    for (int index : indices) {
        Vertex vertex{};
        vertex.x = x + f.p[index][0];
        // Classic liquid bounds extend from y - 0.1 to y + 0.9.
        vertex.y = y + (f.p[index][1] > 0.5f ? 0.9f : -0.1f);
        vertex.z = z + f.p[index][2];
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

    for (int layer = 0;
         layer < LAYERS;
         ++layer) {
        buildVertices_[layer].clear();
        buildWaterVertices_[layer].clear();
        buildLavaVertices_[layer].clear();
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

                if (tile->isLiquid()) {
                    for (int face = 0; face < 6; ++face) {
                        const Face& f = faces[face];
                        const int nx = x + f.nx;
                        const int ny = y + f.ny;
                        const int nz = z + f.nz;

                        if (nx < 0 || nx >= level.width() ||
                            nz < 0 || nz >= level.height() || ny < 0)
                            continue;
                        if (level.isSolidTile(nx, ny, nz))
                            continue;

                        const int neighborId = level.getTile(nx, ny, nz);
                        if (neighborId > 0 && neighborId < Tile::MAX_TILES) {
                            const Tile* neighbor = Tile::tiles[neighborId];
                            if (neighbor && neighbor->isLiquid() &&
                                neighbor->liquidFamily() == tile->liquidFamily())
                                continue;
                        }

                        const bool lit = level.isLit(nx, ny, nz);
                        const int renderLayer = lit ? 0 : 1;
                        std::vector<Vertex>& liquidVertices =
                            tile->liquidFamily() == 1
                                ? buildWaterVertices_[renderLayer]
                                : buildLavaVertices_[renderLayer];
                        appendFluidFace(
                            liquidVertices,
                            x, y, z, face,
                            lit ? f.shade : f.shade * 0.6f,
                            tile->getTexture(face));
                    }
                    continue;
                }

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

                    // The reference dark pass enables an ambient light
                    // level of 0.6 rather than changing the tile's base
                    // per-face shade.
                    const float brightness =
                        lit
                            ? f.shade
                            : f.shade * 0.6f;

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

    void* newVbo[LAYERS] = {nullptr, nullptr};
    void* newWaterVbo[LAYERS] = {nullptr, nullptr};
    void* newLavaVbo[LAYERS] = {nullptr, nullptr};

    for (int layer = 0; layer < LAYERS; ++layer) {
        const std::size_t solidBytes =
            buildVertices_[layer].size() * sizeof(Vertex);
        const std::size_t waterBytes =
            buildWaterVertices_[layer].size() * sizeof(Vertex);
        const std::size_t lavaBytes =
            buildLavaVertices_[layer].size() * sizeof(Vertex);

        if (solidBytes) {
            newVbo[layer] = linearAlloc(solidBytes);
            if (!newVbo[layer])
                goto allocation_failed;
            std::memcpy(newVbo[layer], buildVertices_[layer].data(), solidBytes);
        }
        if (waterBytes) {
            newWaterVbo[layer] = linearAlloc(waterBytes);
            if (!newWaterVbo[layer])
                goto allocation_failed;
            std::memcpy(
                newWaterVbo[layer], buildWaterVertices_[layer].data(), waterBytes);
        }
        if (lavaBytes) {
            newLavaVbo[layer] = linearAlloc(lavaBytes);
            if (!newLavaVbo[layer])
                goto allocation_failed;
            std::memcpy(
                newLavaVbo[layer], buildLavaVertices_[layer].data(), lavaBytes);
        }
    }

    for (int layer = 0; layer < LAYERS; ++layer) {
        if (chunk.vbo[layer]) linearFree(chunk.vbo[layer]);
        if (chunk.waterVbo[layer]) linearFree(chunk.waterVbo[layer]);
        if (chunk.lavaVbo[layer]) linearFree(chunk.lavaVbo[layer]);

        chunk.vbo[layer] = newVbo[layer];
        chunk.vertexCount[layer] =
            static_cast<int>(buildVertices_[layer].size());
        chunk.waterVbo[layer] = newWaterVbo[layer];
        chunk.waterVertexCount[layer] =
            static_cast<int>(buildWaterVertices_[layer].size());
        chunk.lavaVbo[layer] = newLavaVbo[layer];
        chunk.lavaVertexCount[layer] =
            static_cast<int>(buildLavaVertices_[layer].size());
    }

    chunk.dirty = false;
    return true;

allocation_failed:
    for (int layer = 0; layer < LAYERS; ++layer) {
        if (newVbo[layer]) linearFree(newVbo[layer]);
        if (newWaterVbo[layer]) linearFree(newWaterVbo[layer]);
        if (newLavaVbo[layer]) linearFree(newLavaVbo[layer]);
    }
    releaseChunkMesh(chunk);
    chunk.dirty = true;
    return false;
}

void Renderer::drawWaterChunk(
    const ChunkMesh& chunk,
    int layer) {
    if (!chunk.waterVbo[layer] || chunk.waterVertexCount[layer] <= 0)
        return;

    C3D_BufInfo* info = C3D_GetBufInfo();
    BufInfo_Init(info);
    BufInfo_Add(info, chunk.waterVbo[layer], sizeof(Vertex), 3, 0x210);
    C3D_SetBufInfo(info);
    C3D_DrawArrays(
        GPU_TRIANGLES, 0, static_cast<u32>(chunk.waterVertexCount[layer]));
}

void Renderer::drawLavaChunk(
    const ChunkMesh& chunk,
    int layer) {
    if (!chunk.lavaVbo[layer] || chunk.lavaVertexCount[layer] <= 0)
        return;

    C3D_BufInfo* info = C3D_GetBufInfo();
    BufInfo_Init(info);
    BufInfo_Add(info, chunk.lavaVbo[layer], sizeof(Vertex), 3, 0x210);
    C3D_SetBufInfo(info);
    C3D_DrawArrays(
        GPU_TRIANGLES, 0, static_cast<u32>(chunk.lavaVertexCount[layer]));
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

bool Renderer::ensureCharacterVboCapacity(
    std::size_t required) {
    if (required <= characterVboCapacity_)
        return true;

    const std::size_t doubled =
        characterVboCapacity_ > 0
            ? characterVboCapacity_ * 2u
            : static_cast<std::size_t>(ZOMBIE_VERTEX_COUNT);

    const std::size_t newCapacity =
        std::max(required, doubled);

    void* replacement =
        linearAlloc(
            newCapacity * sizeof(Vertex));

    if (!replacement)
        return false;

    if (characterVbo_)
        linearFree(characterVbo_);

    characterVbo_ = replacement;
    characterVboCapacity_ = newCapacity;
    return true;
}

bool Renderer::ensureParticleVboCapacity(
    std::size_t required) {
    if (required <= particleVboCapacity_)
        return true;

    const std::size_t doubled =
        particleVboCapacity_ > 0
            ? particleVboCapacity_ * 2u
            : required;

    const std::size_t newCapacity =
        std::max(required, doubled);

    void* replacement =
        linearAlloc(
            newCapacity * sizeof(Vertex));

    if (!replacement)
        return false;

    if (particleVbo_)
        linearFree(particleVbo_);

    particleVbo_ = replacement;
    particleVboCapacity_ = newCapacity;
    return true;
}

bool Renderer::ensureHudVboCapacity(std::size_t required) {
    if (required <= hudVboCapacity_)
        return true;

    constexpr std::size_t CAPACITY_STEP = 256;
    const std::size_t newCapacity =
        ((required + CAPACITY_STEP - 1) / CAPACITY_STEP) * CAPACITY_STEP;
    void* replacement = linearAlloc(newCapacity * sizeof(Vertex));
    if (!replacement)
        return false;

    if (hudVbo_)
        linearFree(hudVbo_);
    hudVbo_ = replacement;
    hudVboCapacity_ = newCapacity;
    return true;
}



void Renderer::releaseChunkMesh(
    ChunkMesh& chunk) {

    for (int layer = 0; layer < LAYERS; ++layer) {
        if (chunk.vbo[layer]) {
            linearFree(chunk.vbo[layer]);
            chunk.vbo[layer] = nullptr;
        }
        if (chunk.waterVbo[layer]) {
            linearFree(chunk.waterVbo[layer]);
            chunk.waterVbo[layer] = nullptr;
        }
        if (chunk.lavaVbo[layer]) {
            linearFree(chunk.lavaVbo[layer]);
            chunk.lavaVbo[layer] = nullptr;
        }
        chunk.vertexCount[layer] = 0;
        chunk.waterVertexCount[layer] = 0;
        chunk.lavaVertexCount[layer] = 0;
    }
}

bool Renderer::pick(
    const Level& level,
    const Player& player,
    float alpha) {

    hasHit_ = false;

    // The reference expands the player's bounding box by 3 blocks
    // and only considers tiles inside that selection volume.
    const AABB selectionBox =
        player.boundingBox().grow(3.0f, 3.0f, 3.0f);

    const int minX = static_cast<int>(selectionBox.x0);
    const int maxX =
        static_cast<int>(selectionBox.x1 + 1.0f);
    const int minY = static_cast<int>(selectionBox.y0);
    const int maxY =
        static_cast<int>(selectionBox.y1 + 1.0f);
    const int minZ = static_cast<int>(selectionBox.z0);
    const int maxZ =
        static_cast<int>(selectionBox.z1 + 1.0f);

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
    float alpha,
    float brightness) const {

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
                static_cast<float>(quad.u1) / 63.999f,
                static_cast<float>(quad.v0) / 31.999f
            },
            {
                static_cast<float>(quad.u0) / 63.999f,
                static_cast<float>(quad.v0) / 31.999f
            },
            {
                static_cast<float>(quad.u0) / 63.999f,
                static_cast<float>(quad.v1) / 31.999f
            },
            {
                static_cast<float>(quad.u1) / 63.999f,
                static_cast<float>(quad.v1) / 31.999f
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

        const AABB zombieBounds =
            zombie.boundingBox();

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

    const std::size_t offset =
        static_cast<std::size_t>(characterVertexCount_);
    const std::size_t end = offset + required;
    if (end > characterVboCapacity_)
        return;

    auto* destination =
        static_cast<unsigned char*>(characterVbo_) +
        offset * sizeof(Vertex);
    std::memcpy(
        destination,
        characterVertices_.data(),
        required * sizeof(Vertex));

    C3D_TexBind(
        0,
        &characterTexture_);

    C3D_BufInfo* bufInfo =
        C3D_GetBufInfo();

    BufInfo_Init(bufInfo);
    BufInfo_Add(
        bufInfo,
        destination,
        sizeof(Vertex),
        3,
        0x210);
    C3D_SetBufInfo(bufInfo);

    C3D_DrawArrays(
        GPU_TRIANGLES,
        0,
        static_cast<u32>(required));

    characterVertexCount_ = static_cast<int>(end);

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

    const double yaw =
        static_cast<double>(player.yRot()) *
        3.14159265358979323846 / 180.0;
    const double pitch =
        static_cast<double>(player.xRot()) *
        3.14159265358979323846 / 180.0;

    const float xa =
        -static_cast<float>(std::cos(yaw));
    const float za =
        -static_cast<float>(std::sin(yaw));
    const float xa2 =
        -za * static_cast<float>(std::sin(pitch));
    const float za2 =
        xa * static_cast<float>(std::sin(pitch));
    const float ya =
        static_cast<float>(std::cos(pitch));

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

    const std::size_t offset = particleVertexCount_;
    const std::size_t end = offset + required;
    if (end > particleVboCapacity_)
        return;

    auto* destination =
        static_cast<unsigned char*>(particleVbo_) +
        offset * sizeof(Vertex);
    std::memcpy(
        destination,
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
        destination,
        sizeof(Vertex),
        3,
        0x210);
    C3D_SetBufInfo(bufInfo);

    C3D_DrawArrays(
        GPU_TRIANGLES,
        0,
        static_cast<u32>(required));

    particleVertexCount_ = end;
}

void Renderer::renderHud(
    int selectedTileId) {

    if (selectedTileId <= 0 ||
        selectedTileId >= Tile::MAX_TILES) {
        return;
    }

    Tile* tile =
        Tile::tiles[selectedTileId];

    if (!tile)
        return;

    const C3D_Mtx savedProjection =
        projection_;

    const float screenWidth = 400.0f;
    const float screenHeight = 240.0f;

    Mtx_OrthoTilt(
        &projection_,
        0.0f,
        screenWidth,
        screenHeight,
        0.0f,
        100.0f,
        300.0f,
        false);

    C3D_Mtx modelView;
    Mtx_Identity(&modelView);

    Mtx_Translate(
        &modelView,
        screenWidth - 16.0f,
        16.0f,
        -200.0f,
        true);

    Mtx_Scale(
        &modelView,
        16.0f,
        16.0f,
        16.0f);

    Mtx_RotateX(
        &modelView,
        -30.0f * PI / 180.0f,
        true);

    Mtx_RotateY(
        &modelView,
        45.0f * PI / 180.0f,
        true);

    Mtx_Translate(
        &modelView,
        -1.5f,
        0.5f,
        0.5f,
        true);

    Mtx_Scale(
        &modelView,
        -1.0f,
        -1.0f,
        -1.0f);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        projectionLocation_,
        &projection_);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        modelViewLocation_,
        &modelView);

    C3D_DepthTest(
        false,
        GPU_ALWAYS,
        GPU_WRITE_ALL);

    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    C3D_TexBind(
        0,
        &terrainTexture_);

    C3D_TexEnv* env =
        C3D_GetTexEnv(0);

    const C3D_TexEnv savedEnv = *env;

    C3D_TexEnvSrc(
        env,
        C3D_Both,
        GPU_TEXTURE0,
        GPU_PRIMARY_COLOR);

    C3D_TexEnvFunc(
        env,
        C3D_Both,
        GPU_MODULATE);

    C3D_ImmDrawBegin(GPU_TRIANGLES);

    auto sendVertex =
        [](float x, float y, float z,
           float r, float g, float b,
           float u, float v) {
            C3D_ImmSendAttrib(
                x, y, z, 1.0f);
            C3D_ImmSendAttrib(
                r, g, b, 1.0f);
            C3D_ImmSendAttrib(
                u, v, 0.0f, 0.0f);
        };

    if (tile->isCrossPlant()) {
        constexpr int textureId = 15;
        const float minU =
            static_cast<float>(textureId % 16) / 16.0f;
        const float maxU =
            minU + 0.0624375f;
        const float minV =
            static_cast<float>(textureId / 16) / 16.0f;
        const float maxV =
            minV + 0.0624375f;

        const float uvs[4][2] = {
            {maxU, minV},
            {minU, minV},
            {minU, maxV},
            {maxU, maxV}
        };

        for (int r = 0; r < 2; ++r) {
            const float angle =
                static_cast<float>(r) * PI / 2.0f +
                0.7853981633974483f;

            const float xa =
                std::sin(angle) * 0.5f;
            const float za =
                std::cos(angle) * 0.5f;

            const float x0 =
                -2.0f + 0.5f - xa;
            const float x1 =
                -2.0f + 0.5f + xa;
            const float y0 = 0.0f;
            const float y1 = 1.0f;
            const float z0 =
                0.5f - za;
            const float z1 =
                0.5f + za;

            const float p[4][3] = {
                {x0, y1, z0},
                {x1, y1, z1},
                {x1, y0, z1},
                {x0, y0, z0}
            };

            const int first[6] =
                {0, 1, 2, 0, 2, 3};

            for (int i : first) {
                sendVertex(
                    p[i][0], p[i][1], p[i][2],
                    1.0f, 1.0f, 1.0f,
                    uvs[i][0], uvs[i][1]);
            }

            const float q[4][3] = {
                {x1, y1, z1},
                {x0, y1, z0},
                {x0, y0, z0},
                {x1, y0, z1}
            };

            const int second[6] =
                {0, 1, 2, 0, 2, 3};

            for (int i : second) {
                sendVertex(
                    q[i][0], q[i][1], q[i][2],
                    1.0f, 1.0f, 1.0f,
                    uvs[i][0], uvs[i][1]);
            }
        }
    } else {
        const int indices[6] =
            {0, 1, 2, 0, 2, 3};

        for (int face = 0; face < 6; ++face) {
            const Face& f =
                faces[face];

            const int textureId =
                tile->getTexture(face);

            const float minU =
                static_cast<float>(
                    textureId % 16) / 16.0f;
            const float maxU =
                minU + 0.0624375f;
            const float minV =
                static_cast<float>(
                    textureId / 16) / 16.0f;
            const float maxV =
                minV + 0.0624375f;

            float u[4] = {};
            float v[4] = {};

            faceUvs(
                face,
                minU, maxU,
                minV, maxV,
                u, v);

            for (int i : indices) {
                const float x =
                    f.p[i][0] - 2.0f;
                const float y =
                    f.p[i][1];
                const float z =
                    f.p[i][2];

                sendVertex(
                    x, y, z,
                    f.shade,
                    f.shade,
                    f.shade,
                    u[i], v[i]);
            }
        }
    }

    C3D_ImmDrawEnd();

    // Draw the centered crosshair in the same HUD pass.
    C3D_TexEnvSrc(
        env,
        C3D_Both,
        GPU_PRIMARY_COLOR,
        GPU_PRIMARY_COLOR);

    C3D_TexEnvFunc(
        env,
        C3D_Both,
        GPU_REPLACE);

    Mtx_Identity(&modelView);

    Mtx_Translate(
        &modelView,
        0.0f,
        0.0f,
        -200.0f,
        true);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        modelViewLocation_,
        &modelView);

    const float cx =
        screenWidth * 0.5f;
    const float cy =
        screenHeight * 0.5f;

    C3D_ImmDrawBegin(GPU_TRIANGLES);

    const float cross[8][3] = {
        {cx + 1.0f, cy - 4.0f, 0.0f},
        {cx + 0.0f, cy - 4.0f, 0.0f},
        {cx + 0.0f, cy + 5.0f, 0.0f},
        {cx + 1.0f, cy + 5.0f, 0.0f},
        {cx + 5.0f, cy + 0.0f, 0.0f},
        {cx - 4.0f, cy + 0.0f, 0.0f},
        {cx - 4.0f, cy + 1.0f, 0.0f},
        {cx + 5.0f, cy + 1.0f, 0.0f}
    };

    const int vertical[6] =
        {0, 1, 2, 0, 2, 3};
    const int horizontal[6] =
        {4, 5, 6, 4, 6, 7};

    for (int i : vertical) {
        C3D_ImmSendAttrib(
            cross[i][0],
            cross[i][1],
            cross[i][2],
            1.0f);
        C3D_ImmSendAttrib(
            1.0f, 1.0f, 1.0f, 1.0f);
        C3D_ImmSendAttrib(
            0.0f, 0.0f, 0.0f, 0.0f);
    }

    for (int i : horizontal) {
        C3D_ImmSendAttrib(
            cross[i][0],
            cross[i][1],
            cross[i][2],
            1.0f);
        C3D_ImmSendAttrib(
            1.0f, 1.0f, 1.0f, 1.0f);
        C3D_ImmSendAttrib(
            0.0f, 0.0f, 0.0f, 0.0f);
    }

    C3D_ImmDrawEnd();

        // Batch the bitmap font into one VBO draw. Emitting each glyph pixel
    // as immediate-mode vertices can exhaust Citro3D's 0x40000-byte command buffer.
    char stats[64];
    std::snprintf(
        stats, sizeof(stats), "%d FPS, %d CHUNK UPDATES",
        hudFps_, hudChunkUpdates_);

    hudVertices_.clear();

    const auto appendTextGlyphs = [&](const char* text,
                                      float startX, float startY,
                                      float red, float green, float blue) {
        float cursorX = startX;
        constexpr int glyphIndices[6] = {0, 1, 2, 0, 2, 3};
        for (const char* character = text; *character; ++character) {
            unsigned int code = static_cast<unsigned char>(*character);
            if (code >= 128u) code = static_cast<unsigned int>('?');
            const float u0 = static_cast<float>(code % 16u) / 16.0f;
            const float v0 = static_cast<float>(code / 16u) / 16.0f;
            const float u1 = u0 + 1.0f / 16.0f;
            const float v1 = v0 + 1.0f / 16.0f;
            const float x1 = cursorX + 8.0f;
            const float y1 = startY + 8.0f;
            const float points[4][4] = {
                {cursorX, startY, u0, v0},
                {x1, startY, u1, v0},
                {x1, y1, u1, v1},
                {cursorX, y1, u0, v1}
            };
            for (int point : glyphIndices) {
                Vertex vertex{};
                vertex.x = points[point][0];
                vertex.y = points[point][1];
                vertex.z = 0.0f;
                vertex.r = red;
                vertex.g = green;
                vertex.b = blue;
                vertex.a = 1.0f;
                vertex.u = points[point][2];
                vertex.v = points[point][3];
                hudVertices_.push_back(vertex);
            }
            cursorX += static_cast<float>(fontWidths_[code]) + 1.0f;
        }
    };

    const auto appendHudText = [&](const char* text, float x, float y) {
        appendTextGlyphs(text, x + 1.0f, y + 1.0f, 0.0f, 0.0f, 0.0f);
        appendTextGlyphs(text, x, y, 1.0f, 1.0f, 1.0f);
    };

    appendHudText("0.0.13a_03", 2.0f, 2.0f);
    appendHudText(stats, 2.0f, 12.0f);

    if (!hudVertices_.empty() &&
        ensureHudVboCapacity(hudVertices_.size())) {
        std::memcpy(
            hudVbo_,
            hudVertices_.data(),
            hudVertices_.size() * sizeof(Vertex));

        // The icon and crosshair above used immediate mode. Rebind the regular
        // vertex-buffer pipeline before drawing text, rather than inheriting any
        // immediate-mode buffer or vertex-attribute state.
        C3D_BindProgram(&program_);
        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4);
        AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2);

        C3D_FVUnifMtx4x4(
            GPU_VERTEX_SHADER, projectionLocation_, &projection_);
        Mtx_Identity(&modelView);
        Mtx_Translate(&modelView, 0.0f, 0.0f, -200.0f, true);
        C3D_FVUnifMtx4x4(
            GPU_VERTEX_SHADER, modelViewLocation_, &modelView);

        C3D_TexBind(0, &fontTexture_);
        C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
        C3D_AlphaBlend(
            GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
            GPU_ONE, GPU_ZERO);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
        C3D_CullFace(GPU_CULL_NONE);

        C3D_TexEnv fontEnv = savedEnv;
        C3D_TexEnvSrc(
            &fontEnv, C3D_Both,
            GPU_TEXTURE0, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(&fontEnv, C3D_Both, GPU_MODULATE);
        C3D_SetTexEnv(0, &fontEnv);

        C3D_BufInfo* info = C3D_GetBufInfo();
        BufInfo_Init(info);
        BufInfo_Add(info, hudVbo_, sizeof(Vertex), 3, 0x210);
        C3D_SetBufInfo(info);

        C3D_DrawArrays(
            GPU_TRIANGLES, 0,
            static_cast<int>(hudVertices_.size()));
    }

    C3D_SetTexEnv(
        0,
        const_cast<C3D_TexEnv*>(&savedEnv));

    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_AlphaTest(true, GPU_GREATER, 0x80);

    projection_ = savedProjection;

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        projectionLocation_,
        &projection_);

    C3D_FVUnifMtx4x4(
        GPU_VERTEX_SHADER,
        modelViewLocation_,
        &modelView);
}

void Renderer::renderHit(int selectedTileId, bool placingMode, const Player& player) {
    if (!hasHit_ || !level_)
        return;

    int boxX = hitResult_.x;
    int boxY = hitResult_.y;
    int boxZ = hitResult_.z;
    if (placingMode) {
        switch (hitResult_.face) {
            case 0: --boxY; break;
            case 1: ++boxY; break;
            case 2: --boxZ; break;
            case 3: ++boxZ; break;
            case 4: --boxX; break;
            case 5: ++boxX; break;
            default: break;
        }
    }

    const auto now = std::chrono::system_clock::now();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

    C3D_AlphaTest(false, GPU_GREATER, 0x80);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    const C3D_TexEnv savedEnv = *env;

    if (!placingMode) {
        // Classic BREAK mode: all visible faces receive a pulsing white fill.
        const float pulse =
            (static_cast<float>(std::sin(
                static_cast<double>(milliseconds) / 100.0)) * 0.2f + 0.4f) * 0.5f;
        constexpr int triangleIndices[6] = {0, 1, 2, 0, 2, 3};

        C3D_AlphaBlend(
            GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE,
            GPU_SRC_ALPHA, GPU_ONE);
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_COLOR);
        C3D_TexEnvSrc(
            env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        C3D_CullFace(GPU_CULL_NONE);

        C3D_ImmDrawBegin(GPU_TRIANGLES);
        for (const Face& face : faces) {
            for (int index : triangleIndices) {
                C3D_ImmSendAttrib(
                    boxX + face.p[index][0] + face.nx * HIGHLIGHT_OFFSET,
                    boxY + face.p[index][1] + face.ny * HIGHLIGHT_OFFSET,
                    boxZ + face.p[index][2] + face.nz * HIGHLIGHT_OFFSET,
                    1.0f);
                C3D_ImmSendAttrib(1.0f, 1.0f, 1.0f, pulse);
                C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
        C3D_ImmDrawEnd();
    } else {
        // Classic PLACE mode previews the selected tile translucently.
        Tile* tile = selectedTileId > 0 && selectedTileId < Tile::MAX_TILES
            ? Tile::tiles[selectedTileId]
            : nullptr;
        if (tile) {
            const float tint =
                static_cast<float>(std::sin(
                    static_cast<double>(milliseconds) / 100.0)) * 0.2f + 0.8f;
            const float previewAlpha =
                static_cast<float>(std::sin(
                    static_cast<double>(milliseconds) / 200.0)) * 0.2f + 0.5f;

            std::vector<Vertex> previewVertices;
            previewVertices.reserve(tile->isCrossPlant() ? 24u : 36u);
            if (tile->isCrossPlant()) {
                // Each crossed bush quad contains both windings, so standard
                // back-face culling still leaves a visible side at every angle.
                appendBush(previewVertices, boxX, boxY, boxZ, 1.0f);
            } else {
                // Preview faces use the same outward winding as opaque blocks.
                // Back-face culling removes hidden cube faces, including the
                // face coplanar with the selected block at the placement seam.
                for (int face = 0; face < 6; ++face) {
                    appendFace(
                        previewVertices, boxX, boxY, boxZ,
                        face, faces[face].shade,
                        tile->getTexture(face));
                }
            }

            C3D_AlphaBlend(
                GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                GPU_ONE, GPU_ZERO);
            C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);
            C3D_TexBind(0, &terrainTexture_);
            C3D_TexEnvSrc(
                env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
            C3D_CullFace(GPU_CULL_BACK_CCW);

            C3D_ImmDrawBegin(GPU_TRIANGLES);
            for (const Vertex& vertex : previewVertices) {
                C3D_ImmSendAttrib(vertex.x, vertex.y, vertex.z, 1.0f);
                C3D_ImmSendAttrib(
                    vertex.r * tint, vertex.g * tint,
                    vertex.b * tint, previewAlpha);
                C3D_ImmSendAttrib(vertex.u, vertex.v, 0.0f, 0.0f);
            }
            C3D_ImmDrawEnd();
        }
    }

    // Black 40%-alpha whole-cube outline. PLACE mode outlines the adjacent cell.
    // Citro3D has no GPU_LINES primitive, so edge lines are thin triangle prisms.
    constexpr float OUTLINE_RADIUS = 0.008f;
    constexpr int EDGE_COUNT = 12;
    constexpr int edgeIndices[EDGE_COUNT][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    constexpr int prismFaces[6][4] = {
        {0,1,2,3}, {4,5,6,7},
        {0,1,5,4}, {3,2,6,7},
        {0,3,7,4}, {1,2,6,5}
    };
    constexpr int triangleIndices[6] = {0,1,2,0,2,3};

    const float x0 = static_cast<float>(boxX);
    const float y0 = static_cast<float>(boxY);
    const float z0 = static_cast<float>(boxZ);
    const float x1 = x0 + 1.0f;
    const float y1 = y0 + 1.0f;
    const float z1 = z0 + 1.0f;
    const float corners[8][3] = {
        {x0,y0,z0}, {x1,y0,z0}, {x1,y0,z1}, {x0,y0,z1},
        {x0,y1,z0}, {x1,y1,z0}, {x1,y1,z1}, {x0,y1,z1}
    };

    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_COLOR);
    C3D_TexEnvSrc(
        env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_CullFace(GPU_CULL_NONE);

    C3D_ImmDrawBegin(GPU_TRIANGLES);
    for (int edge = 0; edge < EDGE_COUNT; ++edge) {
        const float* a = corners[edgeIndices[edge][0]];
        const float* b = corners[edgeIndices[edge][1]];
        float lo[3]{};
        float hi[3]{};

        for (int axis = 0; axis < 3; ++axis) {
            lo[axis] = std::min(a[axis], b[axis]);
            hi[axis] = std::max(a[axis], b[axis]);
            if (hi[axis] - lo[axis] < 0.0001f) {
                lo[axis] -= OUTLINE_RADIUS;
                hi[axis] += OUTLINE_RADIUS;
            }
        }

        const float p[8][3] = {
            {lo[0],lo[1],lo[2]}, {hi[0],lo[1],lo[2]},
            {hi[0],lo[1],hi[2]}, {lo[0],lo[1],hi[2]},
            {lo[0],hi[1],lo[2]}, {hi[0],hi[1],lo[2]},
            {hi[0],hi[1],hi[2]}, {lo[0],hi[1],hi[2]}
        };

        for (const auto& face : prismFaces) {
            for (int index : triangleIndices) {
                const float* vertex = p[face[index]];
                C3D_ImmSendAttrib(vertex[0], vertex[1], vertex[2], 1.0f);
                C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.4f);
                C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }
    C3D_ImmDrawEnd();

    C3D_SetTexEnv(0, const_cast<C3D_TexEnv*>(&savedEnv));
    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_ONE, GPU_ZERO,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_AlphaTest(true, GPU_GREATER, 0x80);
}

void Renderer::configureFog(
    const Level& level,
    const Player& player,
    bool litLayer) {
    const int x = static_cast<int>(player.x());
    const int y = static_cast<int>(player.y() + 0.12f);
    const int z = static_cast<int>(player.z());
    const int tileId = level.getTile(x, y, z);

    const Tile* tile =
        tileId > 0 && tileId < Tile::MAX_TILES
            ? Tile::tiles[tileId]
            : nullptr;
    const int family = tile && tile->isLiquid()
        ? tile->liquidFamily()
        : 0;

    if (family == 1) {
        // PICA fog color is packed R | (G << 8) | (B << 16).
        C3D_FogColor(0x00330505);
        C3D_FogLutBind(&waterFogLut_);
    } else if (family == 2) {
        C3D_FogColor(0x00001A99);
        C3D_FogLutBind(&lavaFogLut_);
    } else if (litLayer) {
        C3D_FogColor(0xFEFBFA);
        C3D_FogLutBind(&dayFogLut_);
    } else {
        C3D_FogColor(0x0E0B0A);
        C3D_FogLutBind(&shadowFogLut_);
    }

    C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
}

void Renderer::renderHorizon(const Level& level) {
    if (!rockTextureLoaded_ || !horizonWaterTextureLoaded_)
        return;

    constexpr int SPAN = 128;
    constexpr int EXTENT = 2048;
    const int worldWidth = level.width();
    const int worldLength = level.height();

    // Build immutable horizon meshes once. Generating thousands of immediate
    // vertices every frame would waste command-buffer bandwidth on the O3DS.
    if (!horizonRockVbo_ || !horizonWaterVbo_ ||
        horizonWidth_ != worldWidth || horizonLength_ != worldLength) {
        std::vector<Vertex> rockVertices;
        std::vector<Vertex> waterVertices;
        const float rockTop = 30.0f;
        const float waterY = 31.9f;

        const auto appendQuad = [](std::vector<Vertex>& output,
                                   const float p[4][3],
                                   float uSpan, float vSpan,
                                   float shade, bool flipV) {
            const int indices[6] = {0, 1, 2, 0, 2, 3};
            float uv[4][2] = {
                {0.0f, 0.0f}, {uSpan, 0.0f},
                {uSpan, vSpan}, {0.0f, vSpan}
            };
            if (flipV) {
                for (auto& coordinate : uv)
                    coordinate[1] = vSpan - coordinate[1];
            }
            for (int index : indices) {
                Vertex vertex{};
                vertex.x = p[index][0];
                vertex.y = p[index][1];
                vertex.z = p[index][2];
                vertex.r = shade;
                vertex.g = shade;
                vertex.b = shade;
                vertex.a = 1.0f;
                vertex.u = uv[index][0];
                vertex.v = uv[index][1];
                output.push_back(vertex);
            }
        };

        // Rock ground: inner world floor at y=0; outside skirt at y=30.
        for (int x = -EXTENT; x < worldWidth + EXTENT; x += SPAN) {
            for (int z = -EXTENT; z < worldLength + EXTENT; z += SPAN) {
                const bool inside =
                    x >= 0 && z >= 0 &&
                    x < worldWidth && z < worldLength;
                const float y = inside ? 0.0f : rockTop;
                const float p[4][3] = {
                    {static_cast<float>(x), y, static_cast<float>(z + SPAN)},
                    {static_cast<float>(x + SPAN), y, static_cast<float>(z + SPAN)},
                    {static_cast<float>(x + SPAN), y, static_cast<float>(z)},
                    {static_cast<float>(x), y, static_cast<float>(z)}
                };
                appendQuad(rockVertices, p, SPAN, SPAN, 1.0f, true);
            }
        }

        // Front and back walls are shaded to 80% in the source renderer.
        for (int x = 0; x < worldWidth; x += SPAN) {
            const int nextX = std::min(x + SPAN, worldWidth);
            const float front[4][3] = {
                {static_cast<float>(x), 0.0f, 0.0f},
                {static_cast<float>(nextX), 0.0f, 0.0f},
                {static_cast<float>(nextX), rockTop, 0.0f},
                {static_cast<float>(x), rockTop, 0.0f}
            };
            const float back[4][3] = {
                {static_cast<float>(x), rockTop, static_cast<float>(worldLength)},
                {static_cast<float>(nextX), rockTop, static_cast<float>(worldLength)},
                {static_cast<float>(nextX), 0.0f, static_cast<float>(worldLength)},
                {static_cast<float>(x), 0.0f, static_cast<float>(worldLength)}
            };
            appendQuad(rockVertices, front, nextX - x, rockTop, 0.8f, false);
            appendQuad(rockVertices, back, nextX - x, rockTop, 0.8f, true);
        }

        // Left and right walls are shaded to 60%.
        for (int z = 0; z < worldLength; z += SPAN) {
            const int nextZ = std::min(z + SPAN, worldLength);
            const float left[4][3] = {
                {0.0f, rockTop, static_cast<float>(z)},
                {0.0f, rockTop, static_cast<float>(nextZ)},
                {0.0f, 0.0f, static_cast<float>(nextZ)},
                {0.0f, 0.0f, static_cast<float>(z)}
            };
            const float right[4][3] = {
                {static_cast<float>(worldWidth), 0.0f, static_cast<float>(z)},
                {static_cast<float>(worldWidth), 0.0f, static_cast<float>(nextZ)},
                {static_cast<float>(worldWidth), rockTop, static_cast<float>(nextZ)},
                {static_cast<float>(worldWidth), rockTop, static_cast<float>(z)}
            };
            appendQuad(rockVertices, left, nextZ - z, rockTop, 0.6f, false);
            appendQuad(rockVertices, right, nextZ - z, rockTop, 0.6f, true);
        }

        // Transparent water tiles over the outer region, 0.1 below y=32.
        for (int x = -EXTENT; x < worldWidth + EXTENT; x += SPAN) {
            for (int z = -EXTENT; z < worldLength + EXTENT; z += SPAN) {
                const bool inside =
                    x >= 0 && z >= 0 &&
                    x < worldWidth && z < worldLength;
                if (inside)
                    continue;
                const float p[4][3] = {
                    {static_cast<float>(x), waterY, static_cast<float>(z + SPAN)},
                    {static_cast<float>(x + SPAN), waterY, static_cast<float>(z + SPAN)},
                    {static_cast<float>(x + SPAN), waterY, static_cast<float>(z)},
                    {static_cast<float>(x), waterY, static_cast<float>(z)}
                };
                appendQuad(waterVertices, p, SPAN, SPAN, 1.0f, true);
            }
        }

        const std::size_t rockBytes = rockVertices.size() * sizeof(Vertex);
        const std::size_t waterBytes = waterVertices.size() * sizeof(Vertex);
        void* newRock = linearAlloc(rockBytes);
        void* newWater = linearAlloc(waterBytes);
        if (!newRock || !newWater) {
            if (newRock) linearFree(newRock);
            if (newWater) linearFree(newWater);
            error_ = "Could not allocate Classic horizon geometry in linear memory.";
            return;
        }
        std::memcpy(newRock, rockVertices.data(), rockBytes);
        std::memcpy(newWater, waterVertices.data(), waterBytes);

        if (horizonRockVbo_) linearFree(horizonRockVbo_);
        if (horizonWaterVbo_) linearFree(horizonWaterVbo_);
        horizonRockVbo_ = newRock;
        horizonRockVertexCount_ = rockVertices.size();
        horizonWaterVbo_ = newWater;
        horizonWaterVertexCount_ = waterVertices.size();
        horizonWidth_ = worldWidth;
        horizonLength_ = worldLength;
    }

    const auto drawHorizonBuffer = [](void* buffer, std::size_t count) {
        if (!buffer || count == 0)
            return;
        C3D_BufInfo* info = C3D_GetBufInfo();
        BufInfo_Init(info);
        BufInfo_Add(info, buffer, sizeof(Vertex), 3, 0x210);
        C3D_SetBufInfo(info);
        C3D_DrawArrays(GPU_TRIANGLES, 0, static_cast<u32>(count));
    };

    // Static VBOs keep the original 3DS from rebuilding the full horizon each frame.
    C3D_CullFace(GPU_CULL_NONE);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_ONE, GPU_ZERO,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_TexBind(0, &rockTexture_);
    drawHorizonBuffer(horizonRockVbo_, horizonRockVertexCount_);

    C3D_TexBind(0, &horizonWaterTexture_);
    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);
    drawHorizonBuffer(horizonWaterVbo_, horizonWaterVertexCount_);

    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_ONE, GPU_ZERO,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_BACK_CCW);
    C3D_TexBind(0, &terrainTexture_);
}

void Renderer::render(
    const Level& level,
    const Player& player,
    const std::vector<Zombie>& zombies,
    const ParticleEngine& particleEngine,
    int selectedTileId,
    float alpha,
    bool placingMode,
    const char* bottomMenuTitle,
    const char* const* bottomMenuEntries,
    int bottomMenuCount,
    int bottomMenuSelected,
    const char* bottomMenuStatus) {

    if (!initialized_ ||
        !terrainTextureLoaded_) {
        return;
    }

    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
        return;

    frameActive_ = true;

    initializeChunks(level);

    // VBO contents referenced by queued draws must not be overwritten within
    // the same frame. Reserve space for both lighting passes up front, then
    // append each pass into a separate range of the shared buffers.
    characterVertexCount_ = 0;
    particleVertexCount_ = 0;
    const bool characterVboReady = ensureCharacterVboCapacity(
        zombies.size() * static_cast<std::size_t>(ZOMBIE_VERTEX_COUNT));
    const bool particleVboReady = ensureParticleVboCapacity(
        particleEngine.particles().size() * 6u);

    // FrameBegin(SYNCDRAW) above ensures previous GPU work is complete
    // before old linear-memory VBOs are released below.
    const float renderX =
        player.renderX(alpha);
    const float renderY =
        player.renderY(alpha);
    const float renderZ =
        player.renderZ(alpha);

    const std::uint64_t hudNowMs = currentTimeMs();
    if (hudWindowStartMs_ == 0) hudWindowStartMs_ = hudNowMs;
    ++hudFrameCount_;
    if (hudNowMs - hudWindowStartMs_ >= 1000u) {
        hudFps_ = static_cast<int>(hudFrameCount_);
        hudFrameCount_ = 0;
        hudChunkUpdates_ = hudChunkUpdatesWindow_;
        hudChunkUpdatesWindow_ = 0;
        hudWindowStartMs_ = hudNowMs;
    }

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

    // Match LevelRenderer.updateDirtyChunks(): visible chunks are
    // preferred, then recently-dirtied chunks, then nearest chunks.
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
    }

    const std::uint64_t nowMs = currentTimeMs();

    for (int rebuild = 0;
         rebuild < MAX_REBUILDS_PER_FRAME;
         ++rebuild) {

        std::size_t rebuildIndex = chunks_.size();
        bool bestVisible = false;
        std::uint64_t bestAgeBucket =
            std::numeric_limits<std::uint64_t>::max();
        float bestDistance =
            std::numeric_limits<float>::infinity();

        for (std::size_t index = 0;
             index < chunks_.size();
             ++index) {

            const ChunkMesh& chunk =
                chunks_[index];

            if (!chunk.dirty)
                continue;

            const float centerX =
                (chunk.minX + chunk.maxX) * 0.5f;
            const float centerY =
                (chunk.minY + chunk.maxY) * 0.5f;
            const float centerZ =
                (chunk.minZ + chunk.maxZ) * 0.5f;

            const float cacheDistanceX =
                centerX - renderX;
            const float cacheDistanceY =
                centerY - renderY;
            const float cacheDistanceZ =
                centerZ - renderZ;

            if (cacheDistanceX * cacheDistanceX +
                cacheDistanceY * cacheDistanceY +
                cacheDistanceZ * cacheDistanceZ >
                meshCacheDistanceSquared) {
                continue;
            }

            const bool candidateVisible =
                visible_[index];

            const std::uint64_t ageBucket =
                (nowMs >= chunk.dirtiedTimeMs
                     ? nowMs - chunk.dirtiedTimeMs
                     : 0) /
                2000u;

            // DirtyChunkSorter compares against the player's actual position,
            // not the interpolated render position.
            const float distanceX =
                centerX - player.x();
            const float distanceY =
                centerY - player.y();
            const float distanceZ =
                centerZ - player.z();

            const float distance =
                distanceX * distanceX +
                distanceY * distanceY +
                distanceZ * distanceZ;

            if (rebuildIndex == chunks_.size() ||
                (candidateVisible && !bestVisible) ||
                (candidateVisible == bestVisible &&
                 (ageBucket < bestAgeBucket ||
                  (ageBucket == bestAgeBucket &&
                   distance < bestDistance)))) {

                rebuildIndex = index;
                bestVisible = candidateVisible;
                bestAgeBucket = ageBucket;
                bestDistance = distance;
            }
        }

        if (rebuildIndex == chunks_.size())
            break;

        if (!rebuildChunk(
                level,
                chunks_[rebuildIndex])) {
            break;
        }
        ++hudChunkUpdatesWindow_;
    }

    C3D_RenderTargetClear(
        target_,
        C3D_CLEAR_ALL,
        CLEAR_COLOR,
        0);

    // Grow entity buffers before the first draw of the frame. Reallocating a
    // linear-memory VBO after the bright pass could free memory still queued
    // for the GPU.
    ensureCharacterVboCapacity(
        zombies.size() *
        static_cast<std::size_t>(ZOMBIE_VERTEX_COUNT));

    ensureParticleVboCapacity(
        particleEngine.particles().size() * 6u);

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

    // Match the reference's daylight exponential fog pass.
    C3D_FogColor(0xFEFBFA);
    C3D_FogLutBind(&dayFogLut_);
    C3D_FogGasMode(
        GPU_FOG,
        GPU_PLAIN_DENSITY,
        false);

    renderHorizon(level);

    // renderHorizon restores the terrain texture but disables fog; reinstate
    // daylight fog for ordinary terrain immediately afterward.
    configureFog(level, player, true);

    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {
        if (visible_[index])
            drawChunk(chunks_[index], 0);
    }

    // Lava is opaque, but its faces must also be visible from inside the fluid.
    C3D_CullFace(GPU_CULL_NONE);
    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {
        if (visible_[index])
            drawLavaChunk(chunks_[index], 0);
    }
    C3D_CullFace(GPU_CULL_BACK_CCW);

    if (characterVboReady) {
        renderZombies(zombies, player, alpha, true);
    }

    if (particleVboReady) {
        renderParticles(particleEngine, player, alpha, true);
    }

    // Switch to the shadow fog LUT and color for the second pass.
    configureFog(level, player, false);

    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {
        if (visible_[index])
            drawChunk(chunks_[index], 1);
    }

    C3D_CullFace(GPU_CULL_NONE);
    for (std::size_t index = 0;
         index < chunks_.size();
         ++index) {
        if (visible_[index])
            drawLavaChunk(chunks_[index], 1);
    }
    C3D_CullFace(GPU_CULL_BACK_CCW);

    // The reference draws entities and particles in both world-lighting
    // passes. Without these shadow-layer calls, anything classified as
    // unlit is filtered out by the daylight pass and never drawn at all.
    if (characterVboReady) {
        renderZombies(zombies, player, alpha, false);
    }

    if (particleVboReady) {
        renderParticles(particleEngine, player, alpha, false);
    }

    C3D_AlphaTest(true, GPU_GREATER, 0x00);
    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);

    const auto renderWaterLayer = [&](int layer, bool litLayer) {
        configureFog(level, player, litLayer);
        std::vector<std::size_t> order;
        order.reserve(chunks_.size());
        for (std::size_t index = 0; index < chunks_.size(); ++index) {
            if (visible_[index] &&
                chunks_[index].waterVertexCount[layer] > 0) {
                order.push_back(index);
            }
        }

        // Transparent water must be drawn back-to-front. Chunk sorting is an
        // approximation of the desktop display-list sorter; it avoids drawing
        // the nearest water surfaces over the distant ones.
        const auto distanceSquared = [&](std::size_t index) {
            const ChunkMesh& chunk = chunks_[index];
            const float dx = (chunk.minX + chunk.maxX) * 0.5f - renderX;
            const float dy = (chunk.minY + chunk.maxY) * 0.5f - renderY;
            const float dz = (chunk.minZ + chunk.maxZ) * 0.5f - renderZ;
            return dx * dx + dy * dy + dz * dz;
        };
        std::sort(order.begin(), order.end(),
            [&](std::size_t lhs, std::size_t rhs) {
                return distanceSquared(lhs) > distanceSquared(rhs);
            });

        for (const std::size_t index : order)
            drawWaterChunk(chunks_[index], layer);
    };

    // Transparent fluid surfaces must be visible from either side. In
    // particular, the underside of the top water face must remain visible
    // while the player is submerged.
    C3D_CullFace(GPU_CULL_NONE);
    renderWaterLayer(0, true);
    renderWaterLayer(1, false);
    C3D_CullFace(GPU_CULL_BACK_CCW);

    C3D_AlphaBlend(
        GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_ONE, GPU_ZERO,
        GPU_ONE, GPU_ZERO);
    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_AlphaTest(true, GPU_GREATER, 0x80);



    C3D_FogLutBind(nullptr);
    C3D_FogGasMode(
        GPU_NO_FOG,
        GPU_PLAIN_DENSITY,
        false);

    renderHit(selectedTileId, placingMode, player);

    renderHud(
        selectedTileId);

    if (bottomMenuTitle && bottomTarget_) {
        // Darken the frozen world behind the pause menu so the paused state
        // is immediately visible without replacing the 3D scene.
        const C3D_Mtx savedProjection = projection_;
        Mtx_OrthoTilt(&projection_, 0.0f, 400.0f, 240.0f, 0.0f,
                      100.0f, 300.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
        drawSolidRect(0.0f, 0.0f, 400.0f, 240.0f,
                      0.0f, 0.0f, 0.0f, 0.48f);
        projection_ = savedProjection;
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);

        C3D_RenderTargetClear(bottomTarget_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
        C3D_FrameDrawOn(bottomTarget_);
        drawBottomMenu(
            bottomMenuTitle, bottomMenuEntries, bottomMenuCount,
            bottomMenuSelected, bottomMenuStatus);
        projection_ = savedProjection;
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
    } else if (bottomTarget_) {
        const C3D_Mtx savedProjection = projection_;
        C3D_RenderTargetClear(bottomTarget_, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
        C3D_FrameDrawOn(bottomTarget_);
        drawGameplayBottom(selectedTileId, placingMode, player);
        projection_ = savedProjection;
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation_, &projection_);
    }

    C3D_FrameEnd(0);
    frameActive_ = false;
}

} // namespace luckee
