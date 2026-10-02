#include "luckee/player.hpp"

#include <cmath>
#include <cstdlib>

#include "luckee/input.hpp"
#include "luckee/level.hpp"

namespace luckee {

Player::Player(Level& level) : level_(level) {
    resetPos();
}

void Player::resetPos() {
    const float x = static_cast<float>(std::rand()) /
                    static_cast<float>(RAND_MAX) * level_.width();
    const float y = static_cast<float>(level_.depth()) + 10.0f;
    const float z = static_cast<float>(std::rand()) /
                    static_cast<float>(RAND_MAX) * level_.height();
    setPos(x, y, z);
}

void Player::setPos(float x, float y, float z) {
    x_ = x;
    y_ = y;
    z_ = z;
    constexpr float w = 0.3f;
    constexpr float h = 0.9f;
    bb_ = AABB(x - w, y - h, z - w, x + w, y + h, z + w);
}

void Player::turn(float deltaX, float deltaY) {
    yRot_ += deltaX * 0.15f;
    xRot_ -= deltaY * 0.15f;

    if (xRot_ < -90.0f) xRot_ = -90.0f;
    if (xRot_ > 90.0f) xRot_ = 90.0f;
}

void Player::tick(const InputState& input) {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;

    float xa = input.moveX;
    float ya = input.moveY;

    if (input.jumpPressed && onGround_) {
        yd_ = 0.12f;
    }

    moveRelative(xa, ya, onGround_ ? 0.02f : 0.005f);

    yd_ -= 0.005f;
    move(xd_, yd_, zd_);

    xd_ *= 0.91f;
    yd_ *= 0.98f;
    zd_ *= 0.91f;

    if (onGround_) {
        xd_ *= 0.8f;
        zd_ *= 0.8f;
    }
}

void Player::move(float xa, float ya, float za) {
    const float xaOrg = xa;
    const float yaOrg = ya;
    const float zaOrg = za;

    const auto cubes = level_.getCubes(bb_.expand(xa, ya, za));

    for (const AABB& cube : cubes) ya = cube.clipYCollide(bb_, ya);
    bb_.move(0.0f, ya, 0.0f);

    for (const AABB& cube : cubes) xa = cube.clipXCollide(bb_, xa);
    bb_.move(xa, 0.0f, 0.0f);

    for (const AABB& cube : cubes) za = cube.clipZCollide(bb_, za);
    bb_.move(0.0f, 0.0f, za);

    onGround_ = yaOrg != ya && yaOrg < 0.0f;

    if (xaOrg != xa) xd_ = 0.0f;
    if (yaOrg != ya) yd_ = 0.0f;
    if (zaOrg != za) zd_ = 0.0f;

    x_ = (bb_.x0 + bb_.x1) / 2.0f;
    y_ = bb_.y0 + 1.62f;
    z_ = (bb_.z0 + bb_.z1) / 2.0f;
}

void Player::moveRelative(float xa, float za, float speed) {
    float dist = xa * xa + za * za;
    if (dist < 0.01f) return;

    dist = speed / std::sqrt(dist);
    const float radians = yRot_ * 3.14159265358979323846f / 180.0f;
    const float sin = std::sin(radians);
    const float cos = std::cos(radians);

    xa *= dist;
    za *= dist;

    xd_ += xa * cos - za * sin;
    zd_ += za * cos + xa * sin;
}

} // namespace luckee
