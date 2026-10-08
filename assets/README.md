# External assets

LuckeeMiner intentionally does not bundle the historical Minecraft prototype assets.

Place the required user-provided files at:

    assets/textures/terrain.png
    assets/textures/char.png

Required asset sizes:

- terrain.png: 256 × 256
- char.png: 64 × 32

The game checks for both files before starting. If an asset is missing, the missing path is shown on the bottom screen and the application waits for START to exit.

The release build expects the same paths on the 3DS SD card:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png

Asset licensing is separate from the LuckeeMiner source license. Only distribute assets that you have permission to distribute.
