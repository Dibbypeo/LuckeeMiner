#pragma once

#include <string>
#include <vector>

namespace luckee::assets {

// Required external assets. These are intentionally not bundled with the game.
std::vector<std::string> findMissingAssets();

} // namespace luckee::assets
