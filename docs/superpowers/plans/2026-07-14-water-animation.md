# Plan: Water Tile Animation

**Goal:** Animate water terrain tiles by cycling through frames every 200 ms. `MapRenderer` maintains a frame counter, marks the map dirty on each advance, and passes a frame offset to `TerrainSprite::coordinatesToFrame` so the visible tile cycles through the spritesheet columns.

**Status:** Not started

---

## Background

Water terrain in AoE2 uses an SLP/PNG with multiple animation frames arranged in the spritesheet. Currently `TerrainSprite::coordinatesToFrame(x, y)` picks a static frame based on tile coordinates. Adding a time-based offset cycles through the available frames.

`MapRenderer::update(Time time)` is called every frame but currently ignores `time` (parameter is commented out as unused in the function body). This is where we add the frame ticker.

---

## Step 1: Add water frame counter to `MapRenderer.h`

```cpp
// In the private section of MapRenderer, after m_elevationHeight:
Time m_lastWaterFrameTime = 0;
int m_waterFrame = 0;
static constexpr Time WATER_FRAME_INTERVAL = 200; // ms between frames
static constexpr int WATER_FRAME_COUNT = 8;       // typical water animation frames
```

---

## Step 2: Update `MapRenderer::update` in `MapRenderer.cpp`

Replace the current stub:

```cpp
bool MapRenderer::update(Time /*time*/)
{
    if (!m_map) {
        return false;
    }
```

With:

```cpp
bool MapRenderer::update(Time time)
{
    if (!m_map) {
        return false;
    }

    // Advance water animation frame every WATER_FRAME_INTERVAL ms
    if (time - m_lastWaterFrameTime >= WATER_FRAME_INTERVAL) {
        m_lastWaterFrameTime = time;
        m_waterFrame = (m_waterFrame + 1) % WATER_FRAME_COUNT;
        m_camChanged = true; // mark dirty so updateTexture() redraws
    }
```

---

## Step 3: Pass water frame to tile rendering in `updateTexture`

In `MapRenderer.cpp`, inside `updateTexture()`, the terrain is drawn via `terrain->texture(mapTile, m_textureTarget)` or `terrain->pngTexture(mapTile, m_textureTarget)`.

We need to pass the frame offset to the terrain sprite. The current call is (approximately line 221):

```cpp
TerrainPtr terrain = AssetManager::Inst()->getTerrain(mapTile.terrainId);
```

Followed by a call to draw the terrain tile.

**Approach A (simplest — no API change):** Temporarily patch the `mapTile` copy passed to `texture()` by modifying `MapTile` to carry a frame offset field. Since `MapTile` is a plain struct this is safe.

### `src/mechanics/MapTile.h` — add frame offset

```cpp
// In struct MapTile:
int waterFrame = 0; // animation frame offset for water tiles
```

### In `MapRenderer::updateTexture()`:

Before calling `terrain->texture(mapTile, ...)`, create a local copy with the frame set:

```cpp
MapTile animatedTile = mapTile;
// Water terrain IDs in AoE2: 1 (water shallow), 2 (water medium), 3 (water deep),
// 26 (water terrain misc) — check isWater() or hardcode range
if (isWaterTerrain(mapTile.terrainId)) {
    animatedTile.waterFrame = m_waterFrame;
}
// Then use animatedTile in texture calls
```

Helper (add as a private method or inline lambda):

```cpp
static bool isWaterTerrain(int terrainId) {
    // AoE2 standard water terrain IDs
    return terrainId == 1 || terrainId == 2 || terrainId == 3 ||
           terrainId == 4 || terrainId == 26;
}
```

---

## Step 4: Use frame offset in `TerrainSprite::coordinatesToFrame`

### `src/resource/TerrainSprite.h`

Modify `coordinatesToFrame` to accept the frame offset from `MapTile`:

```cpp
inline int coordinatesToFrame(int x, int y, int frameOffset = 0) const noexcept {
    if (IS_UNLIKELY(!m_slp && !m_isPng)) {
        return -1;
    }
    if (m_isPng) {
        const int base = (y % m_tileSquareCount) + (x % m_tileSquareCount) * m_tileSquareCount;
        return (base + frameOffset) % std::max(1, m_tileSquareCount * m_tileSquareCount);
    } else {
        const int tileSquareCount = m_tileSquareCount;
        const int base = (y % tileSquareCount) + (x % tileSquareCount) * tileSquareCount;
        return (base + frameOffset) % std::max(1, tileSquareCount * tileSquareCount);
    }
}
```

The default `frameOffset = 0` keeps non-water terrain unchanged. All existing call sites continue to work without modification.

### Update the call sites in `TerrainSprite.cpp` `texture()` and `pngTexture()`:

Pass `tile.waterFrame` where `coordinatesToFrame` is currently called:

```cpp
// Old:
const int frame = coordinatesToFrame(tile.col, tile.row);
// New:
const int frame = coordinatesToFrame(tile.col, tile.row, tile.waterFrame);
```

---

## Step 5: Ensure `MapTile` hash still works

`MapTile` is used as a key in `std::unordered_map<MapTile, Drawable::Image::Ptr> m_textures` inside `TerrainSprite`. If `waterFrame` is included in the hash, different frames will generate different cache entries — which is correct for animated terrain, though it will use more memory (8 frames × number of water tiles).

### `src/mechanics/MapTile.h` — update hash (if MapTile has a custom hash):

```cpp
// In the hash or compareTo function, include waterFrame:
h ^= std::hash<int>()(tile.waterFrame) << 16;
```

If `MapTile` uses a default hash, add a `==` operator override that includes `waterFrame`.

---

## Notes

- `m_camChanged = true` causes the full terrain texture to be redrawn. For large maps this is a full repaint every 200 ms. Since terrain is already rendered to `m_textureTarget` (offscreen), this is an offscreen blit — acceptable cost.
- If performance is a concern, only mark water tiles dirty and composite only those rows. But for now, full redraw every 200 ms is fine.
- `WATER_FRAME_COUNT = 8` is the AoE2 standard. Adjust if the actual SLP has more or fewer frames.

---

## Testing

1. Start a game on a map with water (Arabia, Islands, etc.).
2. Observe water tiles — they should visibly animate with a ripple effect.
3. Land tiles should be completely static.
4. Frame rate should not noticeably drop.
