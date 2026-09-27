#include <3ds.h>
#include <cstdio>

#include "luckee/camera.hpp"
#include "luckee/input.hpp"

int main(int, char**) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, nullptr);

    luckee::Camera camera;

    std::puts("LuckeeMiner - engine foundation\n");
    std::puts("Circle Pad : move");
    std::puts("Touch drag : look / camera");
    std::puts("A           : jump");
    std::puts("L           : break block");
    std::puts("R           : place block\n");
    std::puts("Touch-look camera state is active.");
    std::puts("3D rendering is not implemented yet.");

    while (aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        const luckee::InputState input = luckee::readInput();
        camera.applyLook(input.lookDeltaX, input.lookDeltaY);
        if (down & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}