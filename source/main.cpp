#include <3ds.h>
#include <cstdio>

#include "luckee/camera.hpp"
#include "luckee/input.hpp"
#include "luckee/level.hpp"
#include "luckee/player.hpp"

int main(int, char**) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, nullptr);

    luckee::Level level;
    luckee::Player player(level);
    luckee::Camera camera;

    std::puts("LuckeeMiner - rd-132211 recreation\n");
    std::puts("Source-defined world: 256 x 64 x 256");
    std::puts("Blocks: rock + grass\n");
    std::puts("Circle Pad : move");
    std::puts("Touch drag : look / camera");
    std::puts("A           : jump");
    std::puts("START       : exit\n");

    int frames = 0;

    while (aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        const luckee::InputState input = luckee::readInput();

        camera.applyLook(input.lookDeltaX, input.lookDeltaY);
        player.turn(input.lookDeltaX, input.lookDeltaY);
        player.tick(input);

        if (down & KEY_START) break;

        if ((frames++ & 15) == 0) {
            consoleClear();
            std::printf("LuckeeMiner - rd-132211 recreation\n\n");
            std::printf("World: %d x %d x %d\n",
                         level.width(), level.depth(), level.height());
            std::printf("Player: %.3f %.3f %.3f\n",
                         player.x(), player.y(), player.z());
            std::printf("Rotation: %.2f %.2f\n",
                         player.yRot(), player.xRot());
            std::printf("Grounded: %s\n\n",
                         player.onGround() ? "yes" : "no");
            std::puts("Circle Pad : move");
            std::puts("Touch drag : look");
            std::puts("A           : jump");
            std::puts("START       : exit");
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    level.save();
    gfxExit();
    return 0;
}
