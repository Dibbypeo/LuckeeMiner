#pragma once

#include <3ds.h>

namespace luckee {

struct InputState {
    float moveX = 0.0f;
    float moveY = 0.0f;
    float lookDeltaX = 0.0f;
    float lookDeltaY = 0.0f;

    // Jump and player reset are held controls. Block editing, saving, block
    // selection, and zombie spawning are edge-triggered on the 3DS mapping.
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
