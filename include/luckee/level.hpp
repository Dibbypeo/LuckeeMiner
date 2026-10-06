#pragma once

#include <cstdint>
#include <vector>

#include "luckee/aabb.hpp"
#include "luckee/level_listener.hpp"

namespace luckee {

class Level {
public:
    static constexpr int WIDTH = 256;
    static constexpr int DEPTH = 64;
    static constexpr int HEIGHT = 256;

    Level(int width = WIDTH, int height = HEIGHT, int depth = DEPTH);

    void load();
    void save() const;

    void calcLightDepths(int x0, int y0, int x1, int y1);

    void addListener(LevelListener* listener);
    void removeListener(LevelListener* listener);

    bool isTile(int x, int y, int z) const;
    bool isSolidTile(int x, int y, int z) const;
    bool isLightBlocker(int x, int y, int z) const;

    void getCubes(
        const AABB& box,
        std::vector<AABB>& result) const;
    float getBrightness(int x, int y, int z) const;

    void setTile(int x, int y, int z, int type);

    int width() const { return width_; }
    int height() const { return height_; }
    int depth() const { return depth_; }

private:
    std::size_t index(int x, int y, int z) const;

    int width_;
    int height_;
    int depth_;
    std::vector<std::uint8_t> blocks_;
    std::vector<int> lightDepths_;
    std::vector<LevelListener*> listeners_;
};

} // namespace luckee
