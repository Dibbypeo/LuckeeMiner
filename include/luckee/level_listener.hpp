#pragma once

namespace luckee {

// Receives the same kinds of world invalidation events used by the
// reference renderer. Implementations decide how those changes affect
// cached geometry.
class LevelListener {
public:
    virtual ~LevelListener() = default;

    virtual void tileChanged(int x, int y, int z) = 0;
    virtual void lightColumnChanged(
        int x, int z, int y0, int y1) = 0;
    virtual void allChanged() = 0;
};

} // namespace luckee
