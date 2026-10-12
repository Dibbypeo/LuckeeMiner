#include "luckee/texture_loader.hpp"

#include <png.h>

#include <array>
#include <cstdint>
#include <cstdio>
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

static 
static std::uint16_t readLe16(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return static_cast<std::uint16_t>(bytes[at]) |
           (static_cast<std::uint16_t>(bytes[at + 1]) << 8);
}

static bool decodeClassicGif(
    const char* path,
    std::vector<std::uint8_t>& rgba,
    std::string& error) {

    std::FILE* file = std::fopen(path, "rb");
    if (!file) {
        error = "Could not open GIF font: ";
        error += path;
        return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        error = "Could not seek GIF font.";
        return false;
    }
    const long length = std::ftell(file);
    if (length < 13 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        error = "Invalid GIF font file.";
        return false;
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const std::size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (read != bytes.size()) {
        error = "Could not read complete GIF font.";
        return false;
    }

    if (std::memcmp(bytes.data(), "GIF87a", 6) != 0 &&
        std::memcmp(bytes.data(), "GIF89a", 6) != 0) {
        error = "The font asset is not a GIF file.";
        return false;
    }

    const unsigned logicalWidth = readLe16(bytes, 6);
    const unsigned logicalHeight = readLe16(bytes, 8);
    if (logicalWidth != 128 || logicalHeight != 128) {
        error = "Invalid default.gif: expected 128x128.";
        return false;
    }

    const std::uint8_t screenPacked = bytes[10];
    std::size_t cursor = 13;
    std::vector<std::array<std::uint8_t, 3>> globalPalette;
    if (screenPacked & 0x80u) {
        const std::size_t count = static_cast<std::size_t>(1u << ((screenPacked & 7u) + 1u));
        if (cursor + count * 3u > bytes.size()) {
            error = "Truncated GIF global palette.";
            return false;
        }
        globalPalette.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            globalPalette[i] = {bytes[cursor], bytes[cursor + 1], bytes[cursor + 2]};
            cursor += 3;
        }
    }

    int transparentIndex = -1;
    std::size_t imageX = 0, imageY = 0, imageWidth = 0, imageHeight = 0;
    std::uint8_t imagePacked = 0;
    bool foundImage = false;
    std::vector<std::array<std::uint8_t, 3>> palette;
    std::vector<std::uint8_t> compressed;

    while (cursor < bytes.size()) {
        const std::uint8_t marker = bytes[cursor++];
        if (marker == 0x3Bu)
            break;

        if (marker == 0x21u) {
            if (cursor >= bytes.size()) break;
            const std::uint8_t label = bytes[cursor++];
            if (label == 0xF9u) {
                if (cursor >= bytes.size()) break;
                const std::size_t blockSize = bytes[cursor++];
                if (blockSize > bytes.size() - cursor) break;
                if (blockSize >= 4 && (bytes[cursor] & 1u))
                    transparentIndex = bytes[cursor + 3];
                cursor += blockSize;
                if (cursor < bytes.size() && bytes[cursor] == 0)
                    ++cursor;
            } else {
                bool finished = false;
                while (cursor < bytes.size() && !finished) {
                    const std::size_t blockSize = bytes[cursor++];
                    if (blockSize == 0) {
                        finished = true;
                    } else if (blockSize <= bytes.size() - cursor) {
                        cursor += blockSize;
                    } else {
                        error = "Truncated GIF extension.";
                        return false;
                    }
                }
            }
            continue;
        }

        if (marker != 0x2Cu) {
            error = "Unsupported or invalid GIF data block.";
            return false;
        }

        if (cursor + 9 > bytes.size()) {
            error = "Truncated GIF image descriptor.";
            return false;
        }
        imageX = readLe16(bytes, cursor);
        imageY = readLe16(bytes, cursor + 2);
        imageWidth = readLe16(bytes, cursor + 4);
        imageHeight = readLe16(bytes, cursor + 6);
        imagePacked = bytes[cursor + 8];
        cursor += 9;

        palette = globalPalette;
        if (imagePacked & 0x80u) {
            const std::size_t count = static_cast<std::size_t>(1u << ((imagePacked & 7u) + 1u));
            if (cursor + count * 3u > bytes.size()) {
                error = "Truncated GIF local palette.";
                return false;
            }
            palette.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                palette[i] = {bytes[cursor], bytes[cursor + 1], bytes[cursor + 2]};
                cursor += 3;
            }
        }

        if (imageX != 0 || imageY != 0 ||
            imageWidth != logicalWidth || imageHeight != logicalHeight ||
            palette.empty()) {
            error = "default.gif must contain one full-size 128x128 atlas.";
            return false;
        }
        if (cursor >= bytes.size()) {
            error = "Missing GIF LZW data.";
            return false;
        }

        const unsigned minimumCodeSize = bytes[cursor++];
        if (minimumCodeSize < 2 || minimumCodeSize > 8) {
            error = "Unsupported GIF LZW code size.";
            return false;
        }
        bool dataEnded = false;
        while (cursor < bytes.size() && !dataEnded) {
            const std::size_t blockSize = bytes[cursor++];
            if (blockSize == 0) {
                dataEnded = true;
            } else if (blockSize <= bytes.size() - cursor) {
                compressed.insert(
                    compressed.end(),
                    bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                    bytes.begin() + static_cast<std::ptrdiff_t>(cursor + blockSize));
                cursor += blockSize;
            } else {
                error = "Truncated GIF image data.";
                return false;
            }
        }
        foundImage = true;
        break;
    }

    if (!foundImage) {
        error = "No image found in default.gif.";
        return false;
    }

    const int clearCode = 1 << minimumCodeSize;
    const int endCode = clearCode + 1;
    int codeSize = static_cast<int>(minimumCodeSize) + 1;
    int nextCode = endCode + 1;
    std::array<int, 4096> prefix{};
    std::array<std::uint8_t, 4096> suffix{};
    std::array<std::uint8_t, 4097> stack{};
    for (int i = 0; i < clearCode; ++i)
        suffix[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);

    std::size_t bitOffset = 0;
    const auto readCode = [&](int bits) -> int {
        if (bitOffset + static_cast<std::size_t>(bits) > compressed.size() * 8u)
            return -1;
        int value = 0;
        for (int bit = 0; bit < bits; ++bit) {
            const std::size_t at = bitOffset + static_cast<std::size_t>(bit);
            value |= ((compressed[at >> 3] >> (at & 7u)) & 1u) << bit;
        }
        bitOffset += static_cast<std::size_t>(bits);
        return value;
    };

    std::vector<std::uint8_t> decoded;
    decoded.reserve(logicalWidth * logicalHeight);
    int oldCode = -1;
    std::uint8_t first = 0;
    while (decoded.size() < logicalWidth * logicalHeight) {
        const int inputCode = readCode(codeSize);
        if (inputCode < 0 || inputCode == endCode)
            break;
        if (inputCode == clearCode) {
            codeSize = static_cast<int>(minimumCodeSize) + 1;
            nextCode = endCode + 1;
            oldCode = -1;
            continue;
        }

        int code = inputCode;
        std::size_t stackSize = 0;
        if (code >= nextCode) {
            if (oldCode < 0 || code != nextCode) {
                error = "Invalid GIF LZW dictionary reference.";
                return false;
            }
            stack[stackSize++] = first;
            code = oldCode;
        }

        while (code >= clearCode) {
            if (code >= nextCode || stackSize >= stack.size()) {
                error = "Invalid GIF LZW prefix chain.";
                return false;
            }
            stack[stackSize++] = suffix[static_cast<std::size_t>(code)];
            code = prefix[static_cast<std::size_t>(code)];
        }
        if (code < 0 || code >= clearCode || stackSize >= stack.size()) {
            error = "Invalid GIF LZW literal.";
            return false;
        }
        first = suffix[static_cast<std::size_t>(code)];
        stack[stackSize++] = first;

        while (stackSize > 0)
            decoded.push_back(stack[--stackSize]);

        if (oldCode >= 0 && nextCode < 4096) {
            prefix[static_cast<std::size_t>(nextCode)] = oldCode;
            suffix[static_cast<std::size_t>(nextCode)] = first;
            ++nextCode;
            if (nextCode == (1 << codeSize) && codeSize < 12)
                ++codeSize;
        }
        oldCode = inputCode;
    }

    const std::size_t pixelCount = logicalWidth * logicalHeight;
    if (decoded.size() < pixelCount) {
        error = "GIF font decompressed to too few pixels.";
        return false;
    }

    std::vector<std::uint8_t> indices(pixelCount);
    if (!(imagePacked & 0x40u)) {
        std::memcpy(indices.data(), decoded.data(), pixelCount);
    } else {
        std::size_t source = 0;
        const int starts[4] = {0, 4, 2, 1};
        const int steps[4] = {8, 8, 4, 2};
        for (int pass = 0; pass < 4; ++pass) {
            for (int y = starts[pass]; y < static_cast<int>(logicalHeight); y += steps[pass]) {
                for (unsigned x = 0; x < logicalWidth; ++x) {
                    indices[static_cast<std::size_t>(y) * logicalWidth + x] = decoded[source++];
                }
            }
        }
    }

    rgba.resize(pixelCount * 4u);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const std::uint8_t index = indices[i];
        const std::size_t out = i * 4u;
        if (index < palette.size()) {
            rgba[out + 0] = palette[index][0];
            rgba[out + 1] = palette[index][1];
            rgba[out + 2] = palette[index][2];
        } else {
            rgba[out + 0] = rgba[out + 1] = rgba[out + 2] = 0;
        }
        rgba[out + 3] = static_cast<int>(index) == transparentIndex ? 0 : 255;
    }
    return true;
}


bool loadPng(
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

bool TextureLoader::loadRock(
    const char* path,
    C3D_Tex& texture,
    std::string& error) {
    if (!loadPng(path, 16, 16, "rock.png", texture, error))
        return false;
    C3D_TexSetWrap(&texture, GPU_REPEAT, GPU_REPEAT);
    return true;
}

bool TextureLoader::loadHorizonWater(
    const char* path,
    C3D_Tex& texture,
    std::string& error) {
    if (!loadPng(path, 16, 16, "water.png", texture, error))
        return false;
    C3D_TexSetWrap(&texture, GPU_REPEAT, GPU_REPEAT);
    return true;
}

bool TextureLoader::loadDirt(
    const char* path,
    C3D_Tex& texture,
    std::string& error) {
    if (!loadPng(path, 16, 16, "dirt.png", texture, error))
        return false;
    C3D_TexSetWrap(&texture, GPU_REPEAT, GPU_REPEAT);
    return true;
}

bool TextureLoader::loadFontGif(
    const char* path,
    C3D_Tex& texture,
    std::uint8_t widths[128],
    std::string& error) {
    std::vector<std::uint8_t> rgba;
    if (!decodeClassicGif(path, rgba, error))
        return false;

    constexpr unsigned width = 128;
    constexpr unsigned height = 128;
    for (unsigned ch = 0; ch < 128; ++ch) {
        const unsigned originX = (ch % 16u) * 8u;
        const unsigned originY = (ch / 16u) * 8u;
        unsigned glyphWidth = 8;
        for (unsigned column = 0; column < 8; ++column) {
            bool occupied = false;
            for (unsigned row = 0; row < 8; ++row) {
                const std::size_t pixel =
                    (static_cast<std::size_t>(originY + row) * width +
                     originX + column) * 4u;
                if (rgba[pixel + 2] > 128) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) {
                glyphWidth = column + 1;
                break;
            }
        }
        widths[ch] = static_cast<std::uint8_t>(ch == 32u ? 4u : glyphWidth);
    }

    std::vector<std::uint8_t> gpuData;
    swizzleRgba8(rgba, gpuData, width, height);
    if (!C3D_TexInit(&texture, width, height, GPU_RGBA8)) {
        error = "Could not allocate default.gif font texture.";
        return false;
    }
    C3D_TexSetFilter(&texture, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexUpload(&texture, gpuData.data());
    C3D_TexFlush(&texture);
    return true;
}

} // namespace luckee
