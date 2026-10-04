#include "luckee/input.hpp"

namespace luckee {

InputState readInput() {
    InputState state{};

    circlePosition circle{};
    hidCircleRead(&circle);

    // Match the original game's movement convention:
    //   left/right  -> +/- X
    //   forward     -> negative Z
    //
    // libctru's raw circle-pad Y axis is opposite the conventional game
    // forward axis, so negate it here.
    state.moveX = static_cast<float>(circle.dx) / 156.0f;
    state.moveY = -static_cast<float>(circle.dy) / 156.0f;

    const u32 down = hidKeysDown();
    state.jumpPressed = (down & KEY_A) != 0;
    state.breakPressed = (down & KEY_L) != 0;
    state.placePressed = (down & KEY_R) != 0;

    static bool wasTouching = false;
    static touchPosition previous{};

    touchPosition current{};
    const bool touching =
        (hidKeysHeld() & KEY_TOUCH) != 0;

    if (touching) {
        hidTouchRead(&current);

        if (wasTouching) {
            state.lookDeltaX =
                static_cast<float>(current.px - previous.px);
            state.lookDeltaY =
                static_cast<float>(current.py - previous.py);
        }

        previous = current;
    }

    wasTouching = touching;
    return state;
}

} // namespace luckee
