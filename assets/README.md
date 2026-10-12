# External assets

LuckeeMiner intentionally does not bundle the historical client textures.

For the experimental Classic `c0.0.13a_03` branch, place these files at:

    assets/textures/terrain.png
    assets/textures/char.png
    assets/textures/rock.png
    assets/textures/water.png
    assets/textures/dirt.png
    assets/textures/default.gif

Required sizes:

- `terrain.png`: 256 × 256
- `char.png`: 64 × 32
- `rock.png`: 16 × 16
- `water.png`: 16 × 16 (RGBA; transparency is used by the horizon and fluid renderer)
- `dirt.png`: 16 × 16 (the original Classic dirt tile, repeated for menus/loading screens)
- `default.gif`: 128 × 128 (the original Classic bitmap font atlas; GIF transparency is preserved)

On the SD card, the paths are:

    SD:/3ds/assets/textures/terrain.png
    SD:/3ds/assets/textures/char.png
    SD:/3ds/assets/textures/rock.png
    SD:/3ds/assets/textures/water.png
    SD:/3ds/assets/textures/dirt.png
    SD:/3ds/assets/textures/default.gif

The application checks for missing required files before starting and reports
the exact paths on the bottom screen. The new `dirt.png` and `default.gif`
assets are available in the supplied `c0.0.13a_03.zip`; place them beside the
other Classic textures in the same folder.

Asset licensing is separate from the LuckeeMiner source license; only distribute
texture files you have permission to redistribute.
