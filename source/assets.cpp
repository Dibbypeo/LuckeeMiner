#include "luckee/assets.hpp"

#include <cstdio>

namespace luckee::assets {
namespace {

constexpr const char* requiredAssets[] = {
    "assets/textures/terrain.png",
    "assets/textures/char.png",
    "assets/textures/rock.png",
    "assets/textures/water.png",
    "assets/textures/dirt.png",
    "assets/textures/default.gif"
};

bool fileExists(const char* path) {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    std::fclose(file);
    return true;
}

} // namespace

std::vector<std::string> findMissingAssets() {
    std::vector<std::string> missing;

    for (const char* path : requiredAssets) {
        if (!fileExists(path))
            missing.emplace_back(path);
    }

    return missing;
}

} // namespace luckee::assets
