#pragma once

#include <3ds.h>

namespace luckee {

struct InputState {
    float moveX = 0.0f;
    float moveY = 0.0f;
    float lookDeltaX = 0.0f;
    float lookDeltaY = 0.0f;

    // The reference checks the jump key while held. L/R remain edge-triggered
    // because the reference handles block actions as mouse-button events.
    bool jumpHeld = false;
    bool breakPressed = false;
    bool placePressed = false;
};

InputState readInput();

} // namespace luckee
