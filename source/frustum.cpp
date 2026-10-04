#include "luckee/frustum.hpp"

#include <cmath>

namespace luckee {

void Frustum::set(const C3D_Mtx& clip) {
    // Extract the six clip planes from the row-major projection*view matrix.
    // This is the same plane construction used by the reference Frustum,
    // expressed directly on the matrix instead of querying desktop OpenGL.
    const float* m = clip.m;

    const float rows[6][4] = {
        {m[3] - m[0],  m[7] - m[4],  m[11] - m[8],  m[15] - m[12]}, // right
        {m[3] + m[0],  m[7] + m[4],  m[11] + m[8],  m[15] + m[12]}, // left
        {m[3] + m[1],  m[7] + m[5],  m[11] + m[9],  m[15] + m[13]}, // bottom
        {m[3] - m[1],  m[7] - m[5],  m[11] - m[9],  m[15] - m[13]}, // top
        {m[3] - m[2],  m[7] - m[6],  m[11] - m[10], m[15] - m[14]}, // far
        {m[3] + m[2],  m[7] + m[6],  m[11] + m[10], m[15] + m[14]}  // near
    };

    for (int i = 0; i < 6; ++i) {
        const float length = std::sqrt(
            rows[i][0] * rows[i][0] +
            rows[i][1] * rows[i][1] +
            rows[i][2] * rows[i][2]);

        if (length <= 0.0f)
            continue;

        for (int j = 0; j < 4; ++j)
            planes_[i][j] = rows[i][j] / length;
    }
}

bool Frustum::cubeInFrustum(const AABB& box) const {
    for (int i = 0; i < 6; ++i) {
        // Select the positive vertex for this plane. If even that vertex
        // is outside, the whole AABB is outside.
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
