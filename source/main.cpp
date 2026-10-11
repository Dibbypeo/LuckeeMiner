#include <3ds.h>
#include <cstdio>
#include <cstddef>
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

constexpr std::size_t MAX_ACTIVE_ZOMBIES = 32;

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

void showGenerationProgress(const char* stage, int percent) {
    consoleClear();
    std::printf("Minecraft Classic c0.0.13a_03\n\n");
    std::printf("Generating world: %s\n\n", stage ? stage : "Working");
    constexpr int BAR_WIDTH = 24;
    const int filled = std::max(0, std::min(BAR_WIDTH, percent * BAR_WIDTH / 100));
    std::putchar('[');
    for (int i = 0; i < BAR_WIDTH; ++i)
        std::putchar(i < filled ? '#' : '.');
    std::printf("] %d%%\n", std::max(0, std::min(100, percent)));
    std::fflush(stdout);
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

void runPauseMenu(
    luckee::Level& level,
    luckee::Player& player,
    std::vector<luckee::Zombie>& zombies,
    luckee::ParticleEngine& particles,
    luckee::Renderer& renderer,
    luckee::Timer& timer,
    int& selectedTileId,
    bool& placingMode,
    bool& shouldExit) {
    static const char* const entries[] = {
        "Resume game",
        "Save world",
        "Load saved world",
        "Generate new world",
        "Exit game"
    };
    constexpr int ENTRY_COUNT = 5;
    int selected = 0;
    std::string status;
    bool done = false;

    while (aptMainLoop() && !done && !shouldExit) {
        hidScanInput();
        const u32 down = hidKeysDown();

        if (down & (KEY_B | KEY_START)) {
            done = true;
            continue;
        }
        if (down & KEY_DUP)
            selected = (selected + ENTRY_COUNT - 1) % ENTRY_COUNT;
        if (down & KEY_DDOWN)
            selected = (selected + 1) % ENTRY_COUNT;

        if (down & KEY_A) {
            switch (selected) {
            case 0:
                done = true;
                break;
            case 1:
                status = level.save()
                    ? "Saved level.dat successfully."
                    : "Save failed. Check level.dat and filesystem space.";
                break;
            case 2:
                if (level.load()) {
                    player.resetPosition();
                    zombies.clear();
                    particles.clear();
                    status = "Loaded level.dat.";
                } else {
                    status = "No valid level.dat or level.dat.bak was found.";
                }
                break;
            case 3:
                level.generateNewWorld(&showGenerationProgress);
                player.resetPosition();
                zombies.clear();
                particles.clear();
                selectedTileId = luckee::Tile::rock->id();
                status = "Generated a new Classic world.";
                break;
            case 4:
                shouldExit = true;
                done = true;
                break;
            default:
                break;
            }
        }

        if (!done && !shouldExit) {
            consoleClear();
            std::puts("Minecraft Classic c0.0.13a_03 - Paused\n");
            std::puts("D-Pad: select    A: choose    B/START: resume\n");
            for (int i = 0; i < ENTRY_COUNT; ++i)
                std::printf("%s %s\n", i == selected ? ">" : " ", entries[i]);
            if (!status.empty())
                std::printf("\n%s\n", status.c_str());

            renderer.render(
                level, player, zombies, particles,
                selectedTileId, timer.alpha(), placingMode);
            gspWaitForVBlank();
        }
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

    luckee::Level level(
        luckee::Level::WIDTH,
        luckee::Level::HEIGHT,
        luckee::Level::DEPTH,
        &showGenerationProgress);
    luckee::Player player(level);
    presentStartupStage("Level and player created");

    std::vector<luckee::Zombie> zombies;
    zombies.reserve(MAX_ACTIVE_ZOMBIES);

    // c0.0.13a_03 does not spawn zombies when a world starts.
    // The Y key remains a native 3DS testing convenience.

    luckee::ParticleEngine particleEngine(level);

    int selectedTileId =
        luckee::Tile::rock->id();
    bool placingMode = false;

    presentStartupStage("Particle engine created");

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

    std::puts("LuckeeMiner - Classic c0.0.13a_03\n");
    std::puts("World: 256 x 64 x 256");
    std::puts("Blocks: 1 rock, 2 grass, 3 dirt, 4 stone, 5 wood, 6 bush");
    std::puts("Boundary: 7; water: 8-9; lava: 10-11");
    std::puts("Zombies: 0 initial (32 active maximum)");
    std::puts("Particle engine: active");
    std::puts("Simulation: 20 ticks/sec");
    std::puts("Assets: terrain.png, char.png, rock.png, water.png\n");
    std::puts("Circle Pad : move");
    std::puts("Touch drag : look");
    std::puts("A           : jump");
    std::puts("L           : action (break/place)");
    std::puts("R           : toggle break/place mode");
    std::puts("Mode        : BREAK");
    std::puts("D-Pad Up   : previous block");
    std::puts("D-Pad Down : next block");
    std::puts("Y           : spawn zombie (max 32 active)");
    std::puts("SELECT      : save world");
    std::puts("X           : reset player position");
    std::puts("START       : exit\n");

    int frames = 0;
    bool shouldExit = false;

    while (aptMainLoop()) {
        hidScanInput();

        const u32 down =
            hidKeysDown();

        const luckee::InputState input =
            luckee::readInput();

        if (down & KEY_START) {
            runPauseMenu(
                level, player, zombies, particleEngine,
                renderer, timer, selectedTileId, placingMode, shouldExit);
            if (shouldExit)
                break;
            continue;
        }

        // SELECT is the 3DS equivalent of the reference's manual save key.
        if (input.savePressed) {
            consoleClear();
            std::puts("LuckeeMiner - saving world\n");
            std::puts("Compressing and writing level.dat...");
            std::fflush(stdout);
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();

            const bool saved = level.save();

            std::puts(
                saved
                    ? "\nWorld save completed."
                    : "\nSAVE FAILED. See the diagnostic message above.");
            std::fflush(stdout);
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
        }

        const int selectableBlocks[] = {1, 2, 3, 4, 5, 6};

        if (input.previousBlockPressed ||
            input.nextBlockPressed) {
            int selectedIndex = 0;
            for (int i = 0; i < 6; ++i) {
                if (selectableBlocks[i] == selectedTileId) {
                    selectedIndex = i;
                    break;
                }
            }

            if (input.previousBlockPressed)
                selectedIndex =
                    (selectedIndex + 5) % 6;
            else
                selectedIndex =
                    (selectedIndex + 1) % 6;

            selectedTileId =
                selectableBlocks[selectedIndex];
        }

        if (input.spawnZombiePressed &&
            zombies.size() < MAX_ACTIVE_ZOMBIES) {
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

        // Classic maps the primary action to L and mode switching to R.
        if (input.togglePlacementModePressed)
            placingMode = !placingMode;

        renderer.pick(level, player, timer.alpha());
        if (const luckee::HitResult* hit = renderer.hitResult()) {
            if (input.actionPressed) {
                if (!placingMode) {
                    const int oldId = level.getTile(hit->x,hit->y,hit->z);
                    const bool changed = level.setTile(hit->x,hit->y,hit->z,0);
                    if (changed && oldId >= 0 && oldId < luckee::Tile::MAX_TILES) {
                        luckee::Tile* tile = luckee::Tile::tiles[oldId];
                        if (tile) tile->destroy(level,hit->x,hit->y,hit->z,particleEngine);
                    }
                } else {
                    int x=hit->x,y=hit->y,z=hit->z;
                    switch(hit->face) {
                        case 0: --y; break; case 1: ++y; break;
                        case 2: --z; break; case 3: ++z; break;
                        case 4: --x; break; case 5: ++x; break;
                        default: break;
                    }
                    if(selectedTileId>0 && selectedTileId<luckee::Tile::MAX_TILES) {
                        luckee::Tile* selectedTile=luckee::Tile::tiles[selectedTileId];
                        if(selectedTile) {
                            const luckee::AABB placementBox=selectedTile->getAABB(x,y,z);
                            bool blocked=placementBox.intersects(player.boundingBox());
                            for(const luckee::Zombie& zombie:zombies) {
                                if(placementBox.intersects(zombie.boundingBox())) {
                                    blocked=true;
                                    break;
                                }
                            }
                            if(!blocked) level.setTile(x,y,z,selectedTileId);
                        }
                    }
                }
            }
        }

        if ((frames++ & 15) == 0) {
            consoleClear();

            std::printf(
                "LuckeeMiner - Classic c0.0.13a_03\n\n");

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
                "Zombies: %u/%u\n",
                static_cast<unsigned int>(zombies.size()),
                static_cast<unsigned int>(MAX_ACTIVE_ZOMBIES));

            std::printf(
                "Particles: %u/%u\n\n",
                static_cast<unsigned int>(particleEngine.particles().size()),
                static_cast<unsigned int>(luckee::ParticleEngine::MAX_ACTIVE_PARTICLES));

            std::puts("Circle Pad : move");
            std::puts("Touch drag : look");
            std::puts("A           : jump");
            std::puts("L           : action (break/place)");
            std::puts("R           : toggle break/place mode");
            std::printf("Mode        : %s\n", placingMode ? "PLACE" : "BREAK");
            std::puts("D-Pad Up    : previous block");
            std::puts("D-Pad Down  : next block");
            std::puts("Y           : spawn zombie (max 32 active)");
            std::puts("SELECT      : save world");
            std::puts("X           : reset player position");
            std::puts("START       : pause/menu");
        }

        renderer.render(
            level,
            player,
            zombies,
            particleEngine,
            selectedTileId,
            timer.alpha(),
            placingMode);

        gspWaitForVBlank();
    }

    level.removeListener(&renderer);
    level.save();
    renderer.shutdown();

    gfxExit();
    return 0;
}
