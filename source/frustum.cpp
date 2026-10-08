#include "luckee/frustum.hpp"

#include <cmath>

namespace luckee {

void Frustum::set(const C3D_Mtx& clip) {
    // C3D_Mtx exposes logical row components through x/y/z/w. Do not use
    // the raw m[] array because C3D_FVec stores its components in PICA order.
    const float r0[4] = {
        clip.r[0].x, clip.r[0].y, clip.r[0].z, clip.r[0].w
    };
    const float r1[4] = {
        clip.r[1].x, clip.r[1].y, clip.r[1].z, clip.r[1].w
    };
    const float r2[4] = {
        clip.r[2].x, clip.r[2].y, clip.r[2].z, clip.r[2].w
    };
    const float r3[4] = {
        clip.r[3].x, clip.r[3].y, clip.r[3].z, clip.r[3].w
    };

    // Mtx_PerspTilt() uses the 3DS depth range [-1, 0] for the
    // right-handed projection. The side planes are the usual homogeneous
    // clip planes; the near plane is z + w >= 0 and the far plane is z <= 0.
    const float rows[6][4] = {
        {r3[0] - r0[0], r3[1] - r0[1], r3[2] - r0[2], r3[3] - r0[3]}, // right
        {r3[0] + r0[0], r3[1] + r0[1], r3[2] + r0[2], r3[3] + r0[3]}, // left
        {r3[0] + r1[0], r3[1] + r1[1], r3[2] + r1[2], r3[3] + r1[3]}, // bottom
        {r3[0] - r1[0], r3[1] - r1[1], r3[2] - r1[2], r3[3] - r1[3]}, // top
        {-r2[0], -r2[1], -r2[2], -r2[3]},                              // far
        {r3[0] + r2[0], r3[1] + r2[1], r3[2] + r2[2], r3[3] + r2[3]}   // near
    };

    for (int i = 0; i < 6; ++i) {
        const float length =
            static_cast<float>(
                std::sqrt(
                    static_cast<double>(
                        rows[i][0] * rows[i][0] +
                        rows[i][1] * rows[i][1] +
                        rows[i][2] * rows[i][2])));

        if (length <= 0.0f) {
            planes_[i][0] = 0.0f;
            planes_[i][1] = 0.0f;
            planes_[i][2] = 0.0f;
            planes_[i][3] = 1.0f;
            continue;
        }

        for (int j = 0; j < 4; ++j)
            planes_[i][j] = rows[i][j] / length;
    }
}

bool Frustum::cubeInFrustum(const AABB& box) const {
    for (int i = 0; i < 6; ++i) {
        const float x =
            planes_[i][0] >= 0.0f ? box.x1 : box.x0;
        const float y =
            planes_[i][1] >= 0.0f ? box.y1 : box.y0;
        const float z =
            planes_[i][2] >= 0.0f ? box.z1 : box.z0;

        if (planes_[i][0] * x +
            planes_[i][1] * y +
            planes_[i][2] * z +
            planes_[i][3] <= 0.0f) {
            return false;
        }
    }

    return true;
}

} // namespace luckee
