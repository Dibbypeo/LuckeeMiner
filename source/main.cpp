#include <3ds.h>
#include <citro3d.h>
#include <cstdio>

#include "luckee/input.hpp"

int main(int, char**) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, nullptr);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

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

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(nullptr, C3D_CLEAR_ALL, 0x000000FF, 0);
        C3D_FrameEnd(0);
    }

    C3D_Fini();
    gfxExit();
    return 0;
}