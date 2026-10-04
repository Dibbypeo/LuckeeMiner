#pragma once

#include <3ds.h>
#include <citro3d.h>

#include "luckee/aabb.hpp"

namespace luckee {

class Frustum {
public:
    void set(const C3D_Mtx& clip);

    bool cubeInFrustum(const AABB& box) const;

private:
    float planes_[6][4]{};
};

} // namespace luckee
