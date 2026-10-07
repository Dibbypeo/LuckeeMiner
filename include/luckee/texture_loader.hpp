#pragma once

#include <3ds.h>
#include <citro3d.h>

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
};

} // namespace luckee
