#include "luckee/texture_loader.hpp"

#include <png.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace luckee {
namespace {

static u32 mortonInterleave(u32 x, u32 y) {
    static const u32 xLut[] = {
        0x00, 0x01, 0x04, 0x05,
        0x10, 0x11, 0x14, 0x15
    };
    static const u32 yLut[] = {
        0x00, 0x02, 0x08, 0x0A,
        0x20, 0x22, 0x28, 0x2A
    };
    return xLut[x & 7] + yLut[y & 7];
}

// Convert a normal RGBA8 image into the 3DS/PICA200 8x8 Morton texture layout.
// The row flip mirrors the way the original Java code uploaded its BufferedImage
// through OpenGL texture coordinates.
static void swizzleRgba8(
    const std::vector<std::uint8_t>& source,
    std::vector<std::uint8_t>& destination,
    unsigned width,
    unsigned height) {

    const std::size_t bytesPerPixel = 4;
    destination.resize(source.size());

    for (unsigned y = 0; y < height; ++y) {
        const unsigned sourceY = height - 1 - y;

        for (unsigned x = 0; x < width; ++x) {
            const u32 blockX = x & ~7u;
            const u32 offset = (mortonInterleave(x, y) +
                                blockX * 8u) * bytesPerPixel;

            const std::size_t sourceOffset =
                (static_cast<std::size_t>(sourceY) * width + x) * bytesPerPixel;

            destination[offset + 0] = source[sourceOffset + 0];
            destination[offset + 1] = source[sourceOffset + 1];
            destination[offset + 2] = source[sourceOffset + 2];
            destination[offset + 3] = source[sourceOffset + 3];
        }
    }
}

} // namespace

bool TextureLoader::loadTerrain(
    const char* path,
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

    // rd-132211's terrain atlas is 256x256. Keeping this exact prevents
    // accidentally accepting a later/different texture layout.
    if (width != 256 || height != 256) {
        png_image_free(&image);
        error = "Invalid terrain.png: expected 256x256.";
        return false;
    }

    std::vector<std::uint8_t> linear(PNG_IMAGE_SIZE(image));

    if (!png_image_finish_read(
            &image,
            nullptr,
            linear.data(),
            0,
            nullptr)) {
        error = "Could not finish decoding PNG: ";
        error += path;
        png_image_free(&image);
        return false;
    }

    png_image_free(&image);

    std::vector<std::uint8_t> swizzled;
    swizzleRgba8(linear, swizzled, width, height);

    if (!C3D_TexInit(&texture, 256, 256, GPU_RGBA8)) {
        error = "Could not allocate 256x256 GPU texture.";
        return false;
    }

    C3D_TexSetFilter(&texture, GPU_NEAREST, GPU_NEAREST);
    C3D_TexUpload(&texture, swizzled.data());
    C3D_TexFlush(&texture);

    return true;
}

} // namespace luckee
