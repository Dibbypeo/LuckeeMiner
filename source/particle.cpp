#include "luckee/particle.hpp"

#include <cmath>
#include "luckee/java_random.hpp"

namespace luckee {
namespace {
double randomDouble() {
    return mathRandom().nextDouble();
}

float random01() {
    return static_cast<float>(randomDouble());
}
} // namespace

Particle::Particle(
    Level& level,
    float x, float y, float z,
    float xa, float ya, float za,
    int texture)
    // Particles query small collision volumes; avoid reserving 32 AABBs for
    // every short-lived effect. The vector can still grow if a query needs it.
    : Entity(level, 8),
      texture_(texture) {
    setSize(0.2f, 0.2f);
    heightOffset_ = bbHeight_ / 2.0f;
    setPos(x, y, z);

    xd_ = xa + (random01() * 2.0f - 1.0f) * 0.4f;
    yd_ = ya + (random01() * 2.0f - 1.0f) * 0.4f;
    zd_ = za + (random01() * 2.0f - 1.0f) * 0.4f;

    const float speed =
        static_cast<float>(
            (randomDouble() + randomDouble() + 1.0) *
            0.15f);

    const float dd =
        static_cast<float>(
            std::sqrt(
                static_cast<double>(
                    xd_ * xd_ +
                    yd_ * yd_ +
                    zd_ * zd_)));

    if (dd > 0.000001f) {
        xd_ = xd_ / dd * speed * 0.4f;
        yd_ = yd_ / dd * speed * 0.4f + 0.1f;
        zd_ = zd_ / dd * speed * 0.4f;
    }

    uo_ =
        static_cast<float>(randomDouble()) * 3.0f;
    vo_ =
        static_cast<float>(randomDouble()) * 3.0f;
    size_ =
        static_cast<float>(
            randomDouble() * 0.5 + 0.5);
    lifetime_ =
        static_cast<int>(
            4.0 / (randomDouble() * 0.9 + 0.1));
    age_ = 0;

}

void Particle::tick() {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;

    if (age_++ >= lifetime_)
        remove();

    yd_ =
        static_cast<float>(
            static_cast<double>(yd_) - 0.04);

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
