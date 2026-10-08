#include "luckee/entity.hpp"

#include <cmath>
#include <cstdlib>

#include "luckee/level.hpp"

namespace luckee {
namespace {

float random01() {
    return static_cast<float>(std::rand()) /
           static_cast<float>(RAND_MAX);
}

} // namespace

Entity::Entity(Level& level)
    : level_(level) {
    collisionCubes_.reserve(32);
    resetPos();
}

void Entity::resetPos() {
    const float x =
        random01() * static_cast<float>(level_.width());
    // rd-132328 resets entities to three blocks above the level depth.
    const float y =
        static_cast<float>(level_.depth()) + 3.0f;
    const float z =
        random01() * static_cast<float>(level_.height());

    x_ = x;
    y_ = y;
    z_ = z;
    xo_ = x;
    yo_ = y;
    zo_ = z;

    constexpr float w = 0.3f;
    constexpr float h = 0.9f;
    bb_ = AABB(
        x - w, y - h, z - w,
        x + w, y + h, z + w);
}

void Entity::turn(float deltaX, float deltaY) {
    yRot_ += deltaX * 0.15f;
    xRot_ -= deltaY * 0.15f;

    if (xRot_ < -90.0f)
        xRot_ = -90.0f;
    if (xRot_ > 90.0f)
        xRot_ = 90.0f;
}

void Entity::tick() {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;
}

void Entity::move(float xa, float ya, float za) {
    const float xaOrg = xa;
    const float yaOrg = ya;
    const float zaOrg = za;

    level_.getCubes(
        bb_.expand(xa, ya, za),
        collisionCubes_);

    for (const AABB& cube : collisionCubes_)
        ya = cube.clipYCollide(bb_, ya);

    bb_.move(0.0f, ya, 0.0f);

    for (const AABB& cube : collisionCubes_)
        xa = cube.clipXCollide(bb_, xa);

    bb_.move(xa, 0.0f, 0.0f);

    for (const AABB& cube : collisionCubes_)
        za = cube.clipZCollide(bb_, za);

    bb_.move(0.0f, 0.0f, za);

    onGround_ =
        yaOrg != ya &&
        yaOrg < 0.0f;

    if (xaOrg != xa)
        xd_ = 0.0f;
    if (yaOrg != ya)
        yd_ = 0.0f;
    if (zaOrg != za)
        zd_ = 0.0f;

    x_ = (bb_.x0 + bb_.x1) / 2.0f;
    y_ = bb_.y0 + heightOffset_;
    z_ = (bb_.z0 + bb_.z1) / 2.0f;
}

void Entity::moveRelative(float xa, float za, float speed) {
    float dist = xa * xa + za * za;
    if (dist < 0.01f)
        return;

    dist = speed / std::sqrt(dist);

    const float radians =
        yRot_ * 3.14159265358979323846f / 180.0f;
    const float sin = std::sin(radians);
    const float cos = std::cos(radians);

    xa *= dist;
    za *= dist;

    xd_ += xa * cos - za * sin;
    zd_ += za * cos + xa * sin;
}

} // namespace luckee
