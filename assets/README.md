# External assets

LuckeeMiner intentionally does not bundle the historical client textures.

For the experimental Classic `c0.0.13a_03` branch, place these files at:

    assets/textures/terrain.png
    assets/textures/char.png
    assets/textures/rock.png
    assets/textures/water.png

Required sizes:

- `terrain.png`: 256 × 256
- `char.png`: 64 × 32
- `rock.png`: 16 × 16
- `water.png`: 16 × 16 (RGBA; transparency is used by the horizon and fluid renderer)

On the SD card, the paths are:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png
    SD:/3ds/assets/textures/rock.png
    SD:/3ds/assets/textures/water.png

The application checks for missing required files before starting and reports
the exact paths on the bottom screen. Asset licensing is separate from the
LuckeeMiner source license; only distribute texture files you have permission
to redistribute.
