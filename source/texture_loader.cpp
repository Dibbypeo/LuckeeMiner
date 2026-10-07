#include "luckee/texture_loader.hpp"

#include <png.h>

#include <cstdint>
#include <string>
#include <vector>

namespace luckee {
namespace {

static u32 mortonInterleave(u32 x, u32 y) {
    return (x & 1u) |
           ((y & 1u) << 1) |
           ((x & 2u) << 1) |
           ((y & 2u) << 2) |
           ((x & 4u) << 2) |
           ((y & 4u) << 3);
}

static void swizzleRgba8(
    const std::vector<std::uint8_t>& rgba,
    std::vector<std::uint8_t>& gpuData,
    unsigned width,
    unsigned height) {

    constexpr std::size_t bytesPerPixel = 4;
    gpuData.resize(rgba.size());
    const unsigned tilesX = width / 8;

    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const unsigned tileX = x / 8;
            const unsigned tileY = y / 8;
            const unsigned localX = x & 7u;
            const unsigned localY = y & 7u;

            const std::size_t sourceOffset =
                (static_cast<std::size_t>(y) * width + x) *
                bytesPerPixel;

            const std::size_t tileIndex =
                static_cast<std::size_t>(tileY) * tilesX + tileX;

            const std::size_t destinationOffset =
                (tileIndex * 64u +
                 mortonInterleave(localX, localY)) *
                bytesPerPixel;

            gpuData[destinationOffset + 0] = rgba[sourceOffset + 3];
            gpuData[destinationOffset + 1] = rgba[sourceOffset + 2];
            gpuData[destinationOffset + 2] = rgba[sourceOffset + 1];
            gpuData[destinationOffset + 3] = rgba[sourceOffset + 0];
        }
    }
}

static bool loadPng(
    const char* path,
    unsigned expectedWidth,
    unsigned expectedHeight,
    const char* description,
    C3D_Tex& texture,
    std::string& error) {

    png_image image{};
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&image, path)) {
        error = "Could not open/decode PNG: ";
        error += path;
        return false;
    }

    image.format = PNG_FORMAT_RGBA;

    const unsigned width = image.width;
    const unsigned height = image.height;

    if (width != expectedWidth || height != expectedHeight) {
        png_image_free(&image);
        error = "Invalid ";
        error += description;
        error += ": expected ";
        error += std::to_string(expectedWidth);
        error += "x";
        error += std::to_string(expectedHeight);
        error += ".";
        return false;
    }

    std::vector<std::uint8_t> rgba(PNG_IMAGE_SIZE(image));

    if (!png_image_finish_read(
            &image,
            nullptr,
            rgba.data(),
            0,
            nullptr)) {
        error = "Could not finish decoding PNG: ";
        error += path;
        png_image_free(&image);
        return false;
    }

    png_image_free(&image);

    std::vector<std::uint8_t> gpuData;
    swizzleRgba8(rgba, gpuData, width, height);

    if (!C3D_TexInit(
            &texture,
            width,
            height,
            GPU_RGBA8)) {
        error = "Could not allocate texture for ";
        error += description;
        error += ".";
        return false;
    }

    C3D_TexSetFilter(
        &texture,
        GPU_NEAREST,
        GPU_NEAREST);

    C3D_TexUpload(
        &texture,
        gpuData.data());

    C3D_TexFlush(&texture);
    return true;
}

} // namespace

bool TextureLoader::loadTerrain(
    const char* path,
    C3D_Tex& texture,
    std::string& error) {
    return loadPng(
        path,
        256,
        256,
        "terrain.png",
        texture,
        error);
}

bool TextureLoader::loadCharacter(
    const char* path,
    C3D_Tex& texture,
    std::string& error) {
    return loadPng(
        path,
        64,
        32,
        "char.png",
        texture,
        error);
}

} // namespace luckee
