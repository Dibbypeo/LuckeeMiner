#pragma once

#include <3ds.h>

namespace luckee {

struct InputState {
    float moveX = 0.0f;
    float moveY = 0.0f;
    float lookDeltaX = 0.0f;
    float lookDeltaY = 0.0f;

    // The reference checks jump while held. Block editing, saving, and player
    // reset are edge-triggered actions on the 3DS control mapping.
    bool jumpHeld = false;
    bool breakPressed = false;
    bool placePressed = false;
    bool savePressed = false;
    bool resetPressed = false;
    bool nextBlockPressed = false;
    bool previousBlockPressed = false;
    bool spawnZombiePressed = false;
};

InputState readInput();

} // namespace luckee
