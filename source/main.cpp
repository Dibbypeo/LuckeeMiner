#include <3ds.h>
#include <cstdio>

#include "luckee/input.hpp"

int main(int, char**) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, nullptr);

    std::puts("LuckeeMiner - engine foundation\n");
    std::puts("Circle Pad : move");
    std::puts("Touch drag : look / camera");
    std::puts("A           : jump");
    std::puts("L           : break block");
    std::puts("R           : place block\n");
    std::puts("Input loop is active.");
    std::puts("Voxel world systems are next.");

    while (aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        const luckee::InputState input = luckee::readInput();
        (void)input;
        if (down & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}