#include <3ds.h>
#include <cstdio>
#include <cstddef>
#include <algorithm>
#include <string>
#include <vector>

#include "luckee/aabb.hpp"
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
luckee::Renderer* progressRenderer = nullptr;

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
    // Renderer is installed before any world-generation callback can occur.
    if (!progressRenderer)
        return;
    progressRenderer->renderLoadingScreen(stage, percent);
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
            renderer.render(
                level, player, zombies, particles,
                selectedTileId, timer.alpha(), placingMode,
                "Paused", entries, ENTRY_COUNT, selected, status.c_str());
            gspWaitForVBlank();
        }
    }
}

} // namespace

int main(int, char**) {
    gfxInitDefault();

    const std::vector<std::string> missing =
        luckee::assets::findMissingAssets();

    if (!missing.empty()) {
        // Fatal diagnostics use the top screen only. Gameplay never initializes
        // a framebuffer console, leaving the bottom display available for UI.
        consoleInit(GFX_TOP, nullptr);
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

    luckee::Renderer renderer;
    if (!renderer.initialize()) {
        consoleInit(GFX_TOP, nullptr);
        std::puts("LuckeeMiner - renderer error\n");
        if (!renderer.error().empty())
            std::printf("%s\n", renderer.error().c_str());
        else
            std::puts("Could not initialize the renderer and its assets.");
        std::puts("\nPress START to exit.");
        waitForStart();
        renderer.shutdown();
        gfxExit();
        return 1;
    }

    progressRenderer = &renderer;
    int mainMenuSelection = 0;
    bool startGame = false;
    while (aptMainLoop() && !startGame) {
        renderer.renderMainMenu(mainMenuSelection);
        hidScanInput();
        const u32 down = hidKeysDown();
        if (down & KEY_DUP)
            mainMenuSelection = (mainMenuSelection + 1) % 2;
        if (down & KEY_DDOWN)
            mainMenuSelection = (mainMenuSelection + 1) % 2;
        if (down & (KEY_A | KEY_START)) {
            if (mainMenuSelection == 0) {
                startGame = true;
            } else {
                progressRenderer = nullptr;
                renderer.shutdown();
                gfxExit();
                return 0;
            }
        }
        if (down & KEY_B) {
            progressRenderer = nullptr;
            renderer.shutdown();
            gfxExit();
            return 0;
        }
        gspWaitForVBlank();
    }

    if (!startGame) {
        progressRenderer = nullptr;
        renderer.shutdown();
        gfxExit();
        return 0;
    }

    renderer.renderLoadingScreen("Loading world...", 0);
    gspWaitForVBlank();

    luckee::Level level(
        luckee::Level::WIDTH,
        luckee::Level::HEIGHT,
        luckee::Level::DEPTH,
        &showGenerationProgress);
    luckee::Player player(level);

    std::vector<luckee::Zombie> zombies;
    zombies.reserve(MAX_ACTIVE_ZOMBIES);

    // c0.0.13a_03 does not spawn zombies when a world starts.
    // The Y key remains a native 3DS testing convenience.

    luckee::ParticleEngine particleEngine(level);

    int selectedTileId =
        luckee::Tile::rock->id();
    bool placingMode = false;

    renderer.setLevel(level);
    level.addListener(&renderer);

    luckee::Timer timer(20.0f);

    bool shouldExit = false;

    constexpr int selectableBlocks[] = {1, 3, 4, 5, 6};
    constexpr int selectableBlockCount =
        sizeof(selectableBlocks) / sizeof(selectableBlocks[0]);

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

        if (input.previousBlockPressed ||
            input.nextBlockPressed) {
            int selectedIndex = 0;
            for (int i = 0; i < selectableBlockCount; ++i) {
                if (selectableBlocks[i] == selectedTileId) {
                    selectedIndex = i;
                    break;
                }
            }

            if (input.previousBlockPressed)
                selectedIndex =
                    (selectedIndex + selectableBlockCount - 1) %
                    selectableBlockCount;
            else
                selectedIndex =
                    (selectedIndex + 1) % selectableBlockCount;

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
    progressRenderer = nullptr;
    renderer.shutdown();

    gfxExit();
    return 0;
}
