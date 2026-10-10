#include "luckee/player.hpp"

#include "luckee/input.hpp"

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

    if (input.jumpHeld && onGround_)
        yd_ = 0.5f;

    moveRelative(
        xa,
        za,
        onGround_ ? 0.1f : 0.02f);

    yd_ =
        static_cast<float>(
            static_cast<double>(yd_) - 0.08);

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
