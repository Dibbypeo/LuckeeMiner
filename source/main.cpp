#include <3ds.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "luckee/assets.hpp"
#include "luckee/input.hpp"
#include "luckee/level.hpp"
#include "luckee/player.hpp"
#include "luckee/renderer.hpp"
#include "luckee/timer.hpp"
#include "luckee/zombie.hpp"

namespace {

void waitForStart() {
    while (aptMainLoop()) {
        hidScanInput();

        if (hidKeysDown() & KEY_START)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
}

} // namespace

int main(int, char**) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, nullptr);

    // Match Java's per-process Math.random() behavior instead of the
    // deterministic default seed used by std::rand().
    std::srand(static_cast<unsigned int>(osGetTime()));

    const std::vector<std::string> missing =
        luckee::assets::findMissingAssets();

    if (!missing.empty()) {
        std::puts("LuckeeMiner - missing assets\n");
        std::puts("Missing textures:");

        for (const std::string& path : missing)
            std::printf("* %s\n", path.c_str());

        std::puts("\nPlace the files in the assets folder.");
        std::puts("Press START to exit.");

        waitForStart();
        gfxExit();
        return 1;
    }

    luckee::Level level;
    luckee::Player player(level);

    std::vector<luckee::Zombie> zombies;
    zombies.reserve(100);

    for (int i = 0; i < 100; ++i) {
        zombies.emplace_back(
            level,
            128.0f,
            0.0f,
            128.0f);
    }

    luckee::Renderer renderer;

    renderer.setLevel(level);
    level.addListener(&renderer);

    if (!renderer.initialize()) {
        std::puts("LuckeeMiner - asset error\n");

        if (!renderer.error().empty())
            std::printf("%s\n", renderer.error().c_str());
        else
            std::puts("Could not load the required assets.");

        std::puts("\nPress START to exit.");

        level.removeListener(&renderer);
        renderer.shutdown();
        gfxExit();
        return 1;
    }

    luckee::Timer timer(60.0f);

    std::puts("LuckeeMiner - rd-132328 recreation\n");
    std::puts("World: 256 x 64 x 256");
    std::puts("Blocks: rock + grass");
    std::puts("Zombies: 100");
    std::puts("Simulation: 60 ticks/sec");
    std::puts("Assets: terrain.png + char.png\n");
    std::puts("Circle Pad : move");
    std::puts("Touch drag : look");
    std::puts("A           : jump");
    std::puts("L           : place");
    std::puts("R           : break");
    std::puts("START       : exit\n");

    int frames = 0;

    while (aptMainLoop()) {
        hidScanInput();

        const u32 down =
            hidKeysDown();

        const luckee::InputState input =
            luckee::readInput();

        timer.advanceTime();

        // Simulation is fixed at 60 ticks/sec. A render frame can execute
        // multiple simulation ticks when rendering falls behind.
        for (int tick = 0;
             tick < timer.ticks();
             ++tick) {
            for (luckee::Zombie& zombie : zombies)
                zombie.tick();

            player.tick(input);
        }

        // RubyDung turns the player during render, after simulation ticks.
        player.turn(
            input.lookDeltaX,
            input.lookDeltaY);

        // Pick before applying block input, matching the reference's order.
        renderer.pick(
            level,
            player,
            timer.alpha());

        if (const luckee::HitResult* hit =
                renderer.hitResult()) {

            if (input.breakPressed) {
                level.setTile(
                    hit->x,
                    hit->y,
                    hit->z,
                    0);
            }

            if (input.placePressed) {
                int x = hit->x;
                int y = hit->y;
                int z = hit->z;

                switch (hit->face) {
                    case 0: --y; break;
                    case 1: ++y; break;
                    case 2: --z; break;
                    case 3: ++z; break;
                    case 4: --x; break;
                    case 5: ++x; break;
                    default: break;
                }

                level.setTile(
                    x,
                    y,
                    z,
                    1);
            }
        }

        if (down & KEY_START)
            break;

        if ((frames++ & 15) == 0) {
            consoleClear();

            std::printf(
                "LuckeeMiner - rd-132328 recreation\n\n");

            std::printf(
                "World: %d x %d x %d\n",
                level.width(),
                level.depth(),
                level.height());

            std::printf(
                "Player: %.3f %.3f %.3f\n",
                player.x(),
                player.y(),
                player.z());

            std::printf(
                "Rotation: %.2f %.2f\n",
                player.yRot(),
                player.xRot());

            std::printf(
                "Grounded: %s\n",
                player.onGround()
                    ? "yes"
                    : "no");

            std::printf(
                "Simulation ticks: %d\n\n",
                timer.ticks());

            std::puts("Circle Pad : move");
            std::puts("Touch drag : look");
            std::puts("A           : jump");
            std::puts("L           : place");
            std::puts("R           : break");
            std::puts("START       : exit");
        }

        renderer.render(
            level,
            player,
            zombies,
            timer.alpha());

        gspWaitForVBlank();
    }

    level.removeListener(&renderer);
    level.save();
    renderer.shutdown();

    gfxExit();
    return 0;
}
