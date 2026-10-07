#include "luckee/zombie.hpp"

#include <cmath>
#include <cstdlib>

namespace luckee {
namespace {

constexpr float PI = 3.14159265358979323846f;

float random01() {
    return static_cast<float>(std::rand()) /
           static_cast<float>(RAND_MAX);
}

CharacterPart makePart(
    int texX,
    int texY,
    float minX,
    float minY,
    float minZ,
    int width,
    int height,
    int depth) {
    CharacterPart part;
    part.texX = texX;
    part.texY = texY;
    part.minX = minX;
    part.minY = minY;
    part.minZ = minZ;
    part.width = width;
    part.height = height;
    part.depth = depth;
    return part;
}

} // namespace

Zombie::Zombie(Level& level, float x, float y, float z)
    : Entity(level),
      rot_(random01() * PI * 2.0f),
      timeOffs_(random01() * 1239813.0f),
      speed_(1.0f),
      rotA_((random01() + 1.0f) * 0.01f) {
    // rd-132328 overwrites the position fields after Entity construction.
    // Preserve that historical behavior, including the original AABB timing.
    x_ = x;
    y_ = y;
    z_ = z;

    parts_[0] = makePart(0, 0, -4.0f, -8.0f, -4.0f, 8, 8, 8);
    parts_[1] = makePart(16, 16, -4.0f, 0.0f, -2.0f, 8, 12, 4);

    parts_[2] = makePart(40, 16, -3.0f, -2.0f, -2.0f, 4, 12, 4);
    parts_[2].x = -5.0f;
    parts_[2].y = 2.0f;

    parts_[3] = makePart(40, 16, -1.0f, -2.0f, -2.0f, 4, 12, 4);
    parts_[3].x = 5.0f;
    parts_[3].y = 2.0f;

    parts_[4] = makePart(0, 16, -2.0f, 0.0f, -2.0f, 4, 12, 4);
    parts_[4].x = -2.0f;
    parts_[4].y = 12.0f;

    parts_[5] = makePart(0, 16, -2.0f, 0.0f, -2.0f, 4, 12, 4);
    parts_[5].x = 2.0f;
    parts_[5].y = 12.0f;
}

void Zombie::tick() {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;

    rot_ += rotA_;
    rotA_ *= 0.99f;
    rotA_ +=
        (random01() - random01()) *
        random01() *
        random01() *
        0.01f;

    const float xa = std::sin(rot_);
    const float za = std::cos(rot_);

    if (onGround_ && random01() < 0.01f)
        yd_ = 0.12f;

    moveRelative(
        xa,
        za,
        onGround_ ? 0.02f : 0.005f);

    yd_ -= 0.005f;

    move(
        xd_,
        yd_,
        zd_);

    xd_ *= 0.91f;
    yd_ *= 0.98f;
    zd_ *= 0.91f;

    if (y_ > 100.0f)
        resetPos();

    if (onGround_) {
        xd_ *= 0.8f;
        zd_ *= 0.8f;
    }
}

} // namespace luckee
