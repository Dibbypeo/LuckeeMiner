#include "luckee/input.hpp"

namespace luckee {
namespace {

constexpr float CIRCLE_PAD_MAX = 156.0f;
constexpr float CIRCLE_PAD_DEADZONE = 0.18f;

// Use an independent deadzone for each axis. This filters small accidental
// vertical/horizontal offsets even while the other axis is intentionally held.
float applyDeadzone(s16 raw) {
    const float value =
        static_cast<float>(raw) / CIRCLE_PAD_MAX;

    return value > -CIRCLE_PAD_DEADZONE &&
           value < CIRCLE_PAD_DEADZONE
        ? 0.0f
        : value;
}

} // namespace

InputState readInput() {
    InputState state{};

    circlePosition circle{};
    hidCircleRead(&circle);

    // Match the reference movement axes:
    // left/right = +/-X and forward/back = -/+Z.
    state.moveX = applyDeadzone(circle.dx);
    state.moveY = applyDeadzone(circle.dy);

    const u32 down = hidKeysDown();
    const u32 held = hidKeysHeld();

    state.jumpHeld =
        (held & KEY_A) != 0;

    state.placePressed =
        (down & KEY_L) != 0;

    state.breakPressed =
        (down & KEY_R) != 0;

    state.savePressed =
        (down & KEY_SELECT) != 0;

    state.resetPressed =
        (held & KEY_X) != 0;

    state.nextBlockPressed =
        (down & KEY_DDOWN) != 0;

    state.previousBlockPressed =
        (down & KEY_DUP) != 0;

    state.spawnZombiePressed =
        (down & KEY_Y) != 0;

    static bool wasTouching = false;
    static touchPosition previous{};

    touchPosition current{};
    const bool touching =
        (held & KEY_TOUCH) != 0;

    if (touching) {
        hidTouchRead(&current);

        if (wasTouching) {
            state.lookDeltaX =
                static_cast<float>(
                    current.px - previous.px);

            // Preserve the already-correct 3DS touchscreen convention:
            // dragging the stylus upward produces a positive look delta,
            // which Player::turn converts into looking upward.
            state.lookDeltaY =
                -static_cast<float>(
                    current.py - previous.py);
        }

        previous = current;
    }

    wasTouching = touching;
    return state;
}

} // namespace luckee
