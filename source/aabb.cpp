#include "luckee/aabb.hpp"

namespace luckee {

AABB::AABB(float x0_, float y0_, float z0_, float x1_, float y1_, float z1_)
    : x0(x0_), y0(y0_), z0(z0_), x1(x1_), y1(y1_), z1(z1_) {}

AABB AABB::expand(float xa, float ya, float za) const {
    float nx0 = x0, ny0 = y0, nz0 = z0;
    float nx1 = x1, ny1 = y1, nz1 = z1;
    if (xa < 0.0f) nx0 += xa;
    if (xa > 0.0f) nx1 += xa;
    if (ya < 0.0f) ny0 += ya;
    if (ya > 0.0f) ny1 += ya;
    if (za < 0.0f) nz0 += za;
    if (za > 0.0f) nz1 += za;
    return AABB(nx0, ny0, nz0, nx1, ny1, nz1);
}

AABB AABB::grow(float xa, float ya, float za) const {
    return AABB(x0 - xa, y0 - ya, z0 - za,
                x1 + xa, y1 + ya, z1 + za);
}

float AABB::clipXCollide(const AABB& c, float xa) const {
    float max;
    if (c.y1 <= y0 || c.y0 >= y1 || c.z1 <= z0 || c.z0 >= z1) return xa;
    if (xa > 0.0f && c.x1 <= x0 && (max = x0 - c.x1 - epsilon_) < xa) xa = max;
    if (xa < 0.0f && c.x0 >= x1 && (max = x1 - c.x0 + epsilon_) > xa) xa = max;
    return xa;
}

float AABB::clipYCollide(const AABB& c, float ya) const {
    float max;
    if (c.x1 <= x0 || c.x0 >= x1 || c.z1 <= z0 || c.z0 >= z1) return ya;
    if (ya > 0.0f && c.y1 <= y0 && (max = y0 - c.y1 - epsilon_) < ya) ya = max;
    if (ya < 0.0f && c.y0 >= y1 && (max = y1 - c.y0 + epsilon_) > ya) ya = max;
    return ya;
}

float AABB::clipZCollide(const AABB& c, float za) const {
    float max;
    if (c.x1 <= x0 || c.x0 >= x1 || c.y1 <= y0 || c.y0 >= y1) return za;
    if (za > 0.0f && c.z1 <= z0 && (max = z0 - c.z1 - epsilon_) < za) za = max;
    if (za < 0.0f && c.z0 >= z1 && (max = z1 - c.z0 + epsilon_) > za) za = max;
    return za;
}

bool AABB::intersects(const AABB& c) const {
    if (c.x1 <= x0 || c.x0 >= x1) return false;
    if (c.y1 <= y0 || c.y0 >= y1) return false;
    return !(c.z1 <= z0 || c.z0 >= z1);
}

void AABB::move(float xa, float ya, float za) {
    x0 += xa; y0 += ya; z0 += za;
    x1 += xa; y1 += ya; z1 += za;
}

} // namespace luckee
