# Tile Value 2 Blue Texture

This project now renders tile value `2` with a dedicated blue 1-block texture.

## Behavior

- Tile values `1`, `3`, `4`, `10` keep using the normal stage tile texture.
- Tile value `2` uses `assets/texture/maptip/tile_value_2_blue.png` as a full tile image.
- Existing CSV files do not need to change.
- The map editor reflects the change automatically in real time.

## Notes

- The normal stage tile sheet is still handled as a 3x3 split with identical tile sizes.
- Tile value `2` is drawn as a single image per block, not as a 9-slice sheet.
- If the dedicated `tile 2` texture is missing, the renderer falls back to the normal tile texture.
