#pragma once

#include <3ds.h>

namespace luckee {

// Snapshot of the controls used by the original-3DS game loop.
struct InputState {
    float moveX = 0.0f;       // Circle Pad horizontal, normalized to [-1, 1]
    float moveY = 0.0f;       // Circle Pad vertical, normalized to [-1, 1]
    float lookDeltaX = 0.0f;  // Touch drag delta in pixels
    float lookDeltaY = 0.0f;
    bool jumpPressed = false;
    bool breakPressed = false;
    bool placePressed = false;
};

// Call once per frame after hidScanInput(). Touch deltas are generated while
// the stylus is held; consumers decide how strongly to scale camera rotation.
InputState readInput();

} // namespace luckee