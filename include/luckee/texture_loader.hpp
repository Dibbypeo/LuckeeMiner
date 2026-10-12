#pragma once

#include <3ds.h>
#include <citro3d.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace luckee {

class TextureLoader {
public:
    static bool loadTerrain(
        const char* path,
        C3D_Tex& texture,
        std::string& error);

    static bool loadCharacter(
        const char* path,
        C3D_Tex& texture,
        std::string& error);

    static bool loadRock(
        const char* path,
        C3D_Tex& texture,
        std::string& error);

    static bool loadHorizonWater(
        const char* path,
        C3D_Tex& texture,
        std::string& error);

    static bool loadDirt(
        const char* path,
        C3D_Tex& texture,
        std::string& error);

    static bool loadFontGif(
        const char* path,
        C3D_Tex& texture,
        std::uint8_t widths[128],
        std::string& error);
};

} // namespace luckee
