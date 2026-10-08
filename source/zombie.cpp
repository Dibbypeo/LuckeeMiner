#include "luckee/zombie.hpp"

#include <cmath>

#include "luckee/java_random.hpp"

namespace luckee {
namespace {

constexpr float PI = 3.14159265358979323846f;

double randomDouble() {
    return mathRandom().nextDouble();
}

float random01() {
    return static_cast<float>(randomDouble());
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
    : Entity(level) {
    // Match the extracted jar's initialization order: rotA is initialized
    // at field declaration before the constructor initializes timeOffs/rot.
    rotA_ =
        static_cast<float>(
            (randomDouble() + 1.0) *
            0.01);
    setPos(x, y, z);
    timeOffs_ =
        static_cast<float>(
            randomDouble()) *
        1239813.0f;
    rot_ =
        static_cast<float>(
            randomDouble() *
            3.14159265358979323846 *
            2.0);
    speed_ = 1.0f;
    // rd-20090515 explicitly calls resetPos() after constructing each initial
    // zombie. Spawned zombies instead receive the player's current position.
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

    // The prototype removes a zombie for falling into the void but continues
    // processing the remainder of this tick.
    if (y_ < -100.0f)
        remove();

    rot_ += rotA_;
    rotA_ =
        static_cast<float>(
            static_cast<double>(rotA_) * 0.99);

    const double randomDelta =
        (randomDouble() - randomDouble()) *
        randomDouble() *
        randomDouble() *
        0.08;

    rotA_ =
        static_cast<float>(
            static_cast<double>(rotA_) +
            randomDelta);

    const float xa =
        static_cast<float>(
            std::sin(static_cast<double>(rot_)));
    const float za =
        static_cast<float>(
            std::cos(static_cast<double>(rot_)));

    if (onGround_ && randomDouble() < 0.08)
        yd_ = 0.5f;

    moveRelative(
        xa,
        za,
        onGround_ ? 0.1f : 0.02f);

    yd_ -= 0.08f;

    move(
        xd_,
        yd_,
        zd_);

    xd_ *= 0.91f;
    yd_ *= 0.98f;
    zd_ *= 0.91f;

    if (onGround_) {
        xd_ *= 0.7f;
        zd_ *= 0.7f;
    }
}

} // namespace luckee
