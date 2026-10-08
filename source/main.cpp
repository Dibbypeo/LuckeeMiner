#include <3ds.h>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

#include "luckee/assets.hpp"
#include "luckee/input.hpp"
#include "luckee/level.hpp"
#include "luckee/player.hpp"
#include "luckee/particle_engine.hpp"
#include "luckee/tile.hpp"
#include "luckee/renderer.hpp"
#include "luckee/timer.hpp"
#include "luckee/zombie.hpp"

namespace {

void presentStartupStage(const char* stage) {
    consoleClear();
    std::printf("LuckeeMiner startup\n\n%s\n", stage);
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

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

    presentStartupStage("Assets OK");

    luckee::Level level;
    luckee::Player player(level);
    presentStartupStage("Level and player created");

    std::vector<luckee::Zombie> zombies;
    zombies.reserve(32);

    for (int i = 0; i < 10; ++i) {
        // The extracted rd-20090515 jar creates ten zombies at the
        // center-ish position and then explicitly resets each one.
        zombies.emplace_back(
            level,
            128.0f,
            0.0f,
            128.0f);
        zombies.back().resetPosition();
    }

    luckee::ParticleEngine particleEngine(level);

    int selectedTileId =
        luckee::Tile::rock->id();

    presentStartupStage("10 zombies and particle engine created");

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

    presentStartupStage("Renderer initialized");

    luckee::Timer timer(20.0f);

    std::puts("LuckeeMiner - rd-20090515 recreation\n");
    std::puts("World: 256 x 64 x 256");
    std::puts("Blocks: 1 rock, 2 grass, 3 dirt, 4 stone brick, 5 wood, 6 bush");
    std::puts("Zombies: 10 initial");
    std::puts("Particle engine: active");
    std::puts("Simulation: 20 ticks/sec");
    std::puts("Assets: terrain.png + char.png\n");
    std::puts("Circle Pad : move");
    std::puts("Touch drag : look");
    std::puts("A           : jump");
    std::puts("L           : place selected block");
    std::puts("R           : break");
    std::puts("D-Pad Up   : previous block");
    std::puts("D-Pad Down : next block");
    std::puts("Y           : spawn zombie");
    std::puts("SELECT      : save world");
    std::puts("X           : reset player position");
    std::puts("START       : exit\n");

    int frames = 0;

    while (aptMainLoop()) {
        hidScanInput();

        const u32 down =
            hidKeysDown();

        const luckee::InputState input =
            luckee::readInput();

        // SELECT is the 3DS equivalent of the reference's manual save key.
        if (input.savePressed)
            level.save();

        const int selectableBlocks[] = {1, 3, 4, 5, 6};

        if (input.previousBlockPressed ||
            input.nextBlockPressed) {
            int selectedIndex = 0;
            for (int i = 0; i < 5; ++i) {
                if (selectableBlocks[i] == selectedTileId) {
                    selectedIndex = i;
                    break;
                }
            }

            if (input.previousBlockPressed)
                selectedIndex =
                    (selectedIndex + 4) % 5;
            else
                selectedIndex =
                    (selectedIndex + 1) % 5;

            selectedTileId =
                selectableBlocks[selectedIndex];
        }

        if (input.spawnZombiePressed) {
            zombies.emplace_back(
                level,
                player.x(),
                player.y(),
                player.z());
        }

        timer.advanceTime();

        // Simulation is fixed at 20 ticks/sec. A render frame can execute
        // multiple simulation ticks when rendering falls behind.
        for (int tick = 0;
             tick < timer.ticks();
             ++tick) {
            level.tick();
            particleEngine.tick();

            for (luckee::Zombie& zombie : zombies)
                zombie.tick();

            zombies.erase(
                std::remove_if(
                    zombies.begin(),
                    zombies.end(),
                    [](const luckee::Zombie& zombie) {
                        return zombie.removed();
                    }),
                zombies.end());

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
                const int oldId =
                    level.getTile(
                        hit->x,
                        hit->y,
                        hit->z);

                const bool changed =
                    level.setTile(
                        hit->x,
                        hit->y,
                        hit->z,
                        0);

                if (changed &&
                    oldId >= 0 &&
                    oldId < luckee::Tile::MAX_TILES) {
                    luckee::Tile* tile =
                        luckee::Tile::tiles[oldId];

                    if (tile) {
                        tile->destroy(
                            level,
                            hit->x,
                            hit->y,
                            hit->z,
                            particleEngine);
                    }
                }
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
                    selectedTileId);
            }
        }

        if (down & KEY_START)
            break;

        if ((frames++ & 15) == 0) {
            consoleClear();

            std::printf(
                "LuckeeMiner - rd-20090515 recreation\n\n");

            std::printf(
                "World: %d x %d x %d\n",
                level.width(),
                level.depth(),
                level.height());

            std::printf(
                "Selected block: %d\n",
                selectedTileId);

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
                "Simulation ticks: %d\n",

                timer.ticks());

            std::printf(
                "Particles: %u\n\n",
                static_cast<unsigned int>(
                    particleEngine.particles().size()));

            std::puts("Circle Pad : move");
            std::puts("Touch drag : look");
            std::puts("A           : jump");
            std::puts("L           : place selected");
            std::puts("R           : break");
            std::puts("D-Pad Up    : previous block");
            std::puts("D-Pad Down  : next block");
            std::puts("Y           : spawn zombie");
            std::puts("SELECT      : save world");
            std::puts("X           : reset player position");
            std::puts("START       : exit");
        }

        renderer.render(
            level,
            player,
            zombies,
            particleEngine,
            timer.alpha());

        gspWaitForVBlank();
    }

    level.removeListener(&renderer);
    level.save();
    renderer.shutdown();

    gfxExit();
    return 0;
}
