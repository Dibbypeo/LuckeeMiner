#include "luckee/particle.hpp"

#include <cmath>
#include <cstdlib>

namespace luckee {
namespace {

float random01() {
    return static_cast<float>(std::rand()) /
           static_cast<float>(RAND_MAX);
}

} // namespace

Particle::Particle(
    Level& level,
    float x, float y, float z,
    float xa, float ya, float za,
    int texture)
    : Entity(level),
      texture_(texture) {
    setSize(0.2f, 0.2f);
    heightOffset_ = bbHeight_ / 2.0f;
    setPos(x, y, z);

    xd_ = xa + (random01() * 2.0f - 1.0f) * 0.4f;
    yd_ = ya + (random01() * 2.0f - 1.0f) * 0.4f;
    zd_ = za + (random01() * 2.0f - 1.0f) * 0.4f;

    const float speed =
        (random01() + random01() + 1.0f) * 0.15f;

    const float dd =
        std::sqrt(
            xd_ * xd_ +
            yd_ * yd_ +
            zd_ * zd_);

    if (dd > 0.000001f) {
        xd_ = xd_ / dd * speed * 0.4f;
        yd_ = yd_ / dd * speed * 0.4f + 0.1f;
        zd_ = zd_ / dd * speed * 0.4f;
    }

    uo_ = random01() * 3.0f;
    vo_ = random01() * 3.0f;
    size_ = random01() * 0.5f + 0.5f;
    lifetime_ = static_cast<int>(
        4.0f / (random01() * 0.9f + 0.1f));
    age_ = 0;

    xo_ = x_;
    yo_ = y_;
    zo_ = z_;
}

void Particle::tick() {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;

    if (age_++ >= lifetime_)
        remove();

    yd_ -= 0.04f;

    move(
        xd_,
        yd_,
        zd_);

    xd_ *= 0.98f;
    yd_ *= 0.98f;
    zd_ *= 0.98f;

    if (onGround_) {
        xd_ *= 0.7f;
        zd_ *= 0.7f;
    }
}

} // namespace luckee
