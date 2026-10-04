#include "luckee/input.hpp"

namespace luckee {

InputState readInput() {
    InputState state{};

    circlePosition circle{};
    hidCircleRead(&circle);

    // Match the reference movement axes:
    // left/right = +/-X and forward/back = -/+Z.
    state.moveX =
        static_cast<float>(circle.dx) / 156.0f;

    state.moveY =
        -static_cast<float>(circle.dy) / 156.0f;

    const u32 down = hidKeysDown();
    const u32 held = hidKeysHeld();

    state.jumpHeld =
        (held & KEY_A) != 0;

    state.placePressed =
        (down & KEY_L) != 0;

    state.breakPressed =
        (down & KEY_R) != 0;

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
