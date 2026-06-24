# Tile Value 3 Purple Texture

This project now renders tile value `3` with a dedicated purple 1-block texture.

## Behavior

- Tile values `1`, `2`, `4`, `10` keep using their existing textures.
- Tile value `3` uses `assets/texture/maptip/tile_value_3_purple.png` as a full tile image.
- `H` damage platforms use `assets/texture/maptip/tile_value_3_purple.png` for the base block and `assets/texture/maptip/tile_value_h_damage.png` for the damage spikes.
- `V` vanish objects reuse the same purple texture.
- Existing CSV files do not need to change.
- The map editor reflects the change automatically in real time.

## Notes

- Tile value `3` is drawn as a single image per block, not as a 9-slice sheet.
- If the dedicated `tile 3` texture is missing, the renderer falls back to the normal tile texture.
- If the `H` spike texture is missing, the game falls back to the purple texture so the map still renders.
