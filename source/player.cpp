#include "luckee/player.hpp"

#include "luckee/input.hpp"

namespace luckee {

Player::Player(Level& level)
    : Entity(level) {
    heightOffset_ = 1.62f;
}

void Player::tick(const InputState& input) {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;

    float xa = input.moveX;
    float za = input.moveY;

    if (input.jumpHeld && onGround_)
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

    if (onGround_) {
        xd_ *= 0.8f;
        zd_ *= 0.8f;
    }
}

} // namespace luckee
