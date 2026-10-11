#include "luckee/player.hpp"

#include <cmath>

#include "luckee/input.hpp"
#include "luckee/level.hpp"

namespace luckee {

Player::Player(Level& level)
    : Entity(level) {
    heightOffset_ = 1.62f;

    // Entity's base constructor establishes a temporary position before the
    // player-specific eye offset is known. Rebuild the AABB now so its bottom
    // and top match the player's actual feet/height from the first frame.
    setPos(x_, y_, z_);
}

void Player::resetPosition() {
    resetPos();
}

void Player::tick(const InputState& input) {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;

    if (input.resetPressed)
        resetPosition();

    float xa = input.moveX;
    float za = input.moveY;

    const bool inWater =
        level_.containsLiquid(bb_.grow(0.0f, -0.4f, 0.0f), 1);
    const bool inLava = level_.containsLiquid(bb_, 2);

    if (input.jumpHeld) {
        if (inWater || inLava)
            yd_ += 0.04f;
        else if (onGround_ && !jumpWasHeld_)
            yd_ = 0.42f;
    }

    if (inWater) {
        moveRelative(xa, za, 0.02f);
        const float oldX = x_;
        const float oldY = y_;
        const float oldZ = z_;
        const float requestedX = xd_;
        const float requestedZ = zd_;
        move(xd_, yd_, zd_);
        const bool hitWall =
            std::fabs((x_ - oldX) - requestedX) > 0.0001f ||
            std::fabs((z_ - oldZ) - requestedZ) > 0.0001f;
        if (hitWall &&
            level_.canOccupyEntityBox(
                bb_, xd_, yd_ + 0.6f - y_ + oldY, zd_)) {
            yd_ = 0.3f;
        }
        xd_ *= 0.8f;
        yd_ *= 0.8f;
        zd_ *= 0.8f;
        yd_ = static_cast<float>(static_cast<double>(yd_) - 0.02);
    } else if (inLava) {
        moveRelative(xa, za, 0.02f);
        const float oldX = x_;
        const float oldY = y_;
        const float oldZ = z_;
        const float requestedX = xd_;
        const float requestedZ = zd_;
        move(xd_, yd_, zd_);
        const bool hitWall =
            std::fabs((x_ - oldX) - requestedX) > 0.0001f ||
            std::fabs((z_ - oldZ) - requestedZ) > 0.0001f;
        if (hitWall &&
            level_.canOccupyEntityBox(
                bb_, xd_, yd_ + 0.6f - y_ + oldY, zd_)) {
            yd_ = 0.3f;
        }
        xd_ *= 0.5f;
        yd_ *= 0.5f;
        zd_ *= 0.5f;
        yd_ = static_cast<float>(static_cast<double>(yd_) - 0.02);
    } else {
        moveRelative(xa, za, onGround_ ? 0.1f : 0.02f);
        move(xd_, yd_, zd_);
        xd_ *= 0.91f;
        yd_ *= 0.98f;
        zd_ *= 0.91f;
        yd_ = static_cast<float>(static_cast<double>(yd_) - 0.08);

        if (onGround_) {
            xd_ *= 0.6f;
            zd_ *= 0.6f;
        }
    }

    jumpWasHeld_ = input.jumpHeld;
}

} // namespace luckee
