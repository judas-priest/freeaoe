# Rendering & Map Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete rendering system -- water animation for PNG terrains, unit sprite batching via offscreen texture, minimap economic/normal mode terrain differentiation

**Architecture:**
- `MapRenderer` renders terrain tiles to `m_textureTarget` (offscreen `IRenderTarget`), displayed as a single draw call. Water animation already cycles `m_waterFrame` every 200ms for SLP-based terrains (frame offset), but PNG terrains have no animation frames -- they show static tiles.
- `UnitsRenderer` renders each unit individually to the main `renderTarget_` via `GraphicRender::render()`, which calls `Sprite::texture()` per unit. An `m_outlineOverlay` offscreen target exists for selection outlines/health bars only. No batching of base unit sprites.
- `Minimap` has three modes (Diplomatic/Normal/Economic) with `unitColor()` differentiating unit dot colors per mode. Terrain rendering in `update()` is identical across all modes -- it always uses `terrain.Colors[0]` from the palette. Normal mode should use player colors from the `PlayerColour` table. Economic mode should highlight resource objects (trees, gold, stone, farms) with resource-specific terrain dots.
- Water terrain IDs: 1=shallow, 2=medium, 3=deep, 4=ocean, 22=deep, 26=beach. The `genie::Terrain::IsWater` flag also exists in the dat file.
- SDL2 render target: `SdlRenderTarget` wraps `SDL_Texture` with `SDL_TEXTUREACCESS_TARGET` for offscreen rendering.

**Tech Stack:** C++20, SDL2

---

### Task 1: Water animation -- sinusoidal color tint for PNG terrain tiles

Water animation already works for SLP terrains (frame cycling). PNG terrains are static -- they have one image per tile with no animation frames. Add a subtle animated color tint (blue-shift oscillation) to PNG water tiles to simulate wave movement.

**Files:**
- Modify: `src/render/MapRenderer.cpp`

- [ ] **Step 1: Add sinusoidal blue tint to PNG water tiles after rendering them**

In `MapRenderer::updateTexture()`, after drawing the water tile texture at line 248, apply a semi-transparent blue overlay that oscillates with `m_waterFrame` to give PNG water tiles a wave-like shimmer.

Replace the block at lines 235-248 with:

```cpp
            // Animate water tiles by offsetting the frame
            MapTile animatedTile = mapTile;
            bool isWaterTile = false;
            if (mapTile.terrainId == 1 || mapTile.terrainId == 2 || mapTile.terrainId == 3 ||
                mapTile.terrainId == 4 || mapTile.terrainId == 22 || mapTile.terrainId == 26) {
                isWaterTile = true;
                animatedTile.frame = (mapTile.frame + m_waterFrame) % std::max(1, terrain->frameCount());
            }
            const Drawable::Image::Ptr &tileTexture = terrain->texture(animatedTile, m_textureTarget);
            if (!tileTexture->isValid()) {
                invalidIndicator.center = spos + ScreenPos(Constants::TILE_SIZE_HORIZONTAL/2.f, Constants::TILE_SIZE_VERTICAL/2.f);
                m_textureTarget->draw(invalidIndicator);
                continue;
            }

            m_textureTarget->draw(tileTexture, spos);

            // For PNG water tiles (which have no animation frames), overlay a
            // sinusoidal blue tint that oscillates over time to simulate waves.
            if (isWaterTile && terrain->frameCount() <= 1) {
                // Oscillate alpha between 10 and 40 using the 8-frame cycle
                const float phase = static_cast<float>(m_waterFrame) / 8.0f * 2.0f * 3.14159265f;
                // Per-tile phase offset based on position to create a wave pattern
                const float tilePhase = phase + (col * 0.7f + row * 1.1f);
                const uint8_t alpha = static_cast<uint8_t>(25.0f + 15.0f * std::sin(tilePhase));
                const ScreenPos tileCenter = spos + ScreenPos(Constants::TILE_SIZE_HORIZONTAL / 2.f, Constants::TILE_SIZE_VERTICAL / 2.f);
                Drawable::Circle waveOverlay;
                waveOverlay.radius = Constants::TILE_SIZE;
                waveOverlay.pointCount = 4;
                waveOverlay.aspectRatio = 0.5;
                waveOverlay.filled = true;
                waveOverlay.fillColor = Drawable::Color(100, 140, 255, alpha);
                waveOverlay.borderSize = 0;
                waveOverlay.center = tileCenter;
                m_textureTarget->draw(waveOverlay);
            }
```

- [ ] **Step 2: Add cmath include for std::sin**

At the top of `src/render/MapRenderer.cpp`, add after the existing includes (around line 28):

```cpp
#include <cmath>
```

- [ ] **Step 3: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Run the game on a map with water. PNG water tiles should now show a subtle blue shimmer that moves like waves, while SLP water tiles continue using frame-based animation.

- [ ] **Step 4: Commit**

```bash
cd /home/dima/Projects/freeaoe && git add src/render/MapRenderer.cpp && git commit -m "$(cat <<'EOF'
feat: animated wave tint for PNG water terrain tiles

SLP-based water already animates via frame cycling. PNG terrains have no
animation frames, so overlay a sinusoidal blue tint with per-tile phase
offset to create a moving wave shimmer effect.
EOF
)"
```

---

### Task 2: Unit sprite batching -- render all unit base sprites to an offscreen texture

Currently each unit's base sprite is drawn directly to the main render target via individual `renderTarget->draw(image, pos)` calls. Batch all unit base sprite draws into a single offscreen texture, then composite it in one draw call, mirroring how terrain uses `m_textureTarget`.

**Files:**
- Modify: `src/render/UnitsRenderer.h`
- Modify: `src/render/UnitsRenderer.cpp`

- [ ] **Step 1: Add a units batch texture target to UnitsRenderer.h**

Add a new member after `m_outlineOverlay` (line 27):

```cpp
    std::shared_ptr<IRenderTarget> m_unitsBatch;
```

So the private section becomes:

```cpp
private:
    std::weak_ptr<VisibilityMap> m_visibilityMap;
    std::shared_ptr<IRenderTarget> m_outlineOverlay;
    std::shared_ptr<IRenderTarget> m_unitsBatch;
    std::weak_ptr<Player> m_player;
    std::weak_ptr<UnitManager> m_unitManager;
    MapPos m_previousCameraPos;
```

- [ ] **Step 2: Create/resize the batch texture in begin()**

In `UnitsRenderer::begin()`, after the `m_outlineOverlay` creation block (after line 24), add:

```cpp
    if (!m_unitsBatch || m_unitsBatch->getSize() != renderTarget->getSize()) {
        m_unitsBatch = renderTarget->createTextureTarget(renderTarget->getSize());
    }
    m_unitsBatch->clear(Drawable::Transparent);
```

- [ ] **Step 3: Redirect unit base sprite rendering to the batch texture**

In `UnitsRenderer::render()`, change the unit base rendering to draw to `m_unitsBatch` instead of `renderTarget`. Find line 261:

```cpp
        const ScreenPos pos = camera->absoluteScreenPos(unit->position());
        unit->renderer().render(*renderTarget, pos, RenderType::Base);
```

Replace with:

```cpp
        const ScreenPos pos = camera->absoluteScreenPos(unit->position());
        unit->renderer().render(*m_unitsBatch, pos, RenderType::Base);
```

Also redirect shadow rendering at line 81 to the batch texture:

```cpp
                entity->renderer().render(*m_unitsBatch, camera->absoluteScreenPos(entity->position()), RenderType::Shadow);
```

And the "InTheShadows" rendering at line 92:

```cpp
            entity->renderer().render(*m_unitsBatch, camera->absoluteScreenPos(entity->position()), RenderType::InTheShadows);
```

And doppleganger rendering at line 125:

```cpp
            entity->renderer().render(*m_unitsBatch, camera->absoluteScreenPos(entity->position()), RenderType::InTheShadows);
```

And decaying entity rendering at lines 133 and 135:

```cpp
                entity->renderer().render(*m_unitsBatch, camera->absoluteScreenPos(entity->position()), RenderType::Base);
            } else {
                entity->renderer().render(*m_unitsBatch, camera->absoluteScreenPos(entity->position()), RenderType::InTheShadows);
```

And missile rendering at line 297:

```cpp
    for (const Missile::Ptr &missile : visibleMissiles) {
        missile->renderer().render(*m_unitsBatch, camera->absoluteScreenPos(missile->position()), RenderType::Base);
    }
```

And missile shadow at line 107:

```cpp
            entity->renderer().render(*m_unitsBatch, camera->absoluteScreenPos(shadowPosition), RenderType::Shadow);
```

- [ ] **Step 4: Composite the batch texture in display()**

In `UnitsRenderer::display()`, before the outline overlay compositing (before line 363 `m_outlineOverlay->display()`), add:

```cpp
    m_unitsBatch->display();
    renderTarget->draw(m_unitsBatch);
```

Also redirect the move target marker (line 309) and building placement (lines 342-346) to draw to `m_unitsBatch` as well. Replace lines 308-311:

```cpp
    const MoveTargetMarker::Ptr &marker = unitManager->moveTargetMarker();
    marker->renderer->render(*m_unitsBatch,
                                          camera->absoluteScreenPos(marker->position()),
                                          RenderType::Base);
```

And replace lines 341-346 (building placement rendering):

```cpp
        for (const UnplacedBuilding &building : buildingsToPlace) {
            building.graphic->setOrientation(building.orientation);
            building.graphic->render(*m_unitsBatch,
                                        camera->absoluteScreenPos(building.position),
                                        building.canPlace ? RenderType::ConstructAvailable : RenderType::ConstructUnavailable);
        }
```

- [ ] **Step 5: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Run the game, verify units display correctly -- the visual result should be identical to before (units, shadows, missiles all visible). The performance benefit comes from compositing via a single GPU texture blit instead of many individual sprite draws.

- [ ] **Step 6: Commit**

```bash
cd /home/dima/Projects/freeaoe && git add src/render/UnitsRenderer.h src/render/UnitsRenderer.cpp && git commit -m "$(cat <<'EOF'
feat: batch unit sprites via offscreen texture

Render all unit base sprites, shadows, missiles, and building previews
to m_unitsBatch offscreen texture, then composite in a single draw call.
Mirrors the terrain batching approach via m_textureTarget.
EOF
)"
```

---

### Task 3: Minimap Normal mode -- use actual player colors from PlayerColour table

In Normal mode, units should use their player's actual color from the dat file's PlayerColour table, not hardcoded blue/red. The current implementation uses `Drawable::Blue` for human and `Drawable::Red` for enemies.

**Files:**
- Modify: `src/ui/Minimap.cpp`
- Modify: `src/ui/Minimap.h`

- [ ] **Step 1: Add helper to get player minimap color**

In `src/ui/Minimap.cpp`, add a helper function before `Minimap::unitColor()` (before line 152):

```cpp
static Drawable::Color playerMinimapColor(int playerId, const std::shared_ptr<UnitManager> &unitManager)
{
    if (playerId == UnitManager::GaiaID) {
        return Drawable::Color(128, 192, 128);
    }
    // Player colors in AoE2: index into palette 50500
    // Standard player colors: Blue(1), Red(2), Green(3), Yellow(4), Cyan(5), Purple(6), Grey(7), Orange(8)
    static const Drawable::Color playerColors[] = {
        Drawable::Color(0, 0, 255),     // Player 1 - Blue
        Drawable::Color(255, 0, 0),     // Player 2 - Red
        Drawable::Color(0, 180, 0),     // Player 3 - Green
        Drawable::Color(255, 255, 0),   // Player 4 - Yellow
        Drawable::Color(0, 255, 255),   // Player 5 - Cyan
        Drawable::Color(180, 0, 255),   // Player 6 - Purple
        Drawable::Color(180, 180, 180), // Player 7 - Grey
        Drawable::Color(255, 150, 0),   // Player 8 - Orange
    };
    // playerId is 1-based, array is 0-based
    const int index = playerId - 1;
    if (index >= 0 && index < 8) {
        return playerColors[index];
    }
    return Drawable::White;
}
```

- [ ] **Step 2: Update Normal mode in unitColor() to use player colors**

Replace the `MinimapMode::Normal` case in `unitColor()` (lines 168-173):

```cpp
    case MinimapMode::Normal:
        if (isGaia) return Drawable::Color(128, 192, 128);
        return playerMinimapColor(unit->playerId(), m_unitManager);
```

- [ ] **Step 3: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Run the game, press F4 to cycle to Normal mode. Each player's units should now show in their correct player color (blue, red, green, yellow, etc.) instead of just blue/red.

- [ ] **Step 4: Commit**

```bash
cd /home/dima/Projects/freeaoe && git add src/ui/Minimap.cpp && git commit -m "$(cat <<'EOF'
feat: minimap Normal mode uses actual player colors

Instead of hardcoded blue/red, Normal mode now shows each player's units
in their standard AoE2 player color (blue, red, green, yellow, cyan,
purple, grey, orange).
EOF
)"
```

---

### Task 4: Minimap Economic mode -- highlight resource objects on terrain

In Economic mode, Gaia resource objects (trees, gold, stone, berries, deer) should be prominently colored on the minimap to help players spot resources. Currently they show as dim green dots like any Gaia unit.

**Files:**
- Modify: `src/ui/Minimap.cpp`

- [ ] **Step 1: Add resource-type coloring for Gaia units in Economic mode**

In the `unitColor()` function, update the `MinimapMode::Economic` case. Replace lines 175-187:

```cpp
    case MinimapMode::Economic:
        if (isGaia) {
            // Highlight resource objects by type
            const int unitClass = unit->data()->Class;
            // Trees/forests (class 15 = Tree)
            if (unitClass == genie::Unit::Tree) {
                return Drawable::Color(0, 160, 0); // bright green
            }
            // Gold mines (class 32 = GoldMine)
            if (unitClass == genie::Unit::GoldMine) {
                return Drawable::Color(255, 215, 0); // gold
            }
            // Stone mines (class 34 = StoneMine)
            if (unitClass == genie::Unit::StoneMine) {
                return Drawable::Color(180, 180, 180); // grey/silver
            }
            // Berry bushes (class 30 = BerryBush)
            if (unitClass == genie::Unit::BerryBush) {
                return Drawable::Color(220, 50, 50); // red-ish
            }
            // Huntable animals (class 9 = PreyAnimal)
            if (unitClass == genie::Unit::PreyAnimal) {
                return Drawable::Color(200, 150, 80); // tan
            }
            // Other Gaia
            return Drawable::Color(80, 120, 80); // dim green
        }
        if (!isHuman) return playerMinimapColor(unit->playerId(), m_unitManager);
        // Own units: color by carried resource
        if (unit->resources[genie::ResourceType::WoodStorage]  > 0) return Drawable::Color(0, 192, 0);
        if (unit->resources[genie::ResourceType::FoodStorage]  > 0) return Drawable::Color(255, 200, 0);
        if (unit->resources[genie::ResourceType::GoldStorage]  > 0) return Drawable::Color(255, 215, 0);
        if (unit->resources[genie::ResourceType::StoneStorage] > 0) return Drawable::Color(180, 180, 180);
        if (unit->data()->Class == genie::Unit::Civilian) {
            return Drawable::Color(255, 255, 100); // idle villager
        }
        return playerMinimapColor(unit->playerId(), m_unitManager);
```

- [ ] **Step 2: Verify genie::Unit class constants exist**

Check that the class enum values used above exist. If not, use the raw integer constants:

```cpp
// genie::Unit class constants from AGE:
// 9 = Prey Animal
// 15 = Tree
// 30 = Berry Bush
// 32 = Gold Mine
// 34 = Stone Mine
```

If `genie::Unit::Tree` etc. do not exist as named constants, replace them with the integer values:

```cpp
            if (unitClass == 15) { // Tree
            if (unitClass == 32) { // Gold Mine
            if (unitClass == 34) { // Stone Mine
            if (unitClass == 30) { // Berry Bush
            if (unitClass == 9) {  // Prey Animal
```

- [ ] **Step 3: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Run the game, press F4 to cycle to Economic mode. Trees should show bright green, gold mines should show gold/yellow, stone should show grey, berry bushes red, huntable animals tan. Own villagers carrying resources should show the resource color.

- [ ] **Step 4: Commit**

```bash
cd /home/dima/Projects/freeaoe && git add src/ui/Minimap.cpp && git commit -m "$(cat <<'EOF'
feat: minimap Economic mode highlights resources by type

Gaia resources now show distinct colors: trees=green, gold=yellow,
stone=grey, berries=red, huntable animals=tan. Own villagers show
the color of resources they carry.
EOF
)"
```

---

### Task 5: Minimap -- handle MinimapLargeTerrain2 mode and mode indicator text

The minimap skips units with `MinimapLargeTerrain2` (mode 5) and several other modes (6-9). Mode 5 should be handled the same as mode 4 (MinimapLargeTerrain). Also add a small text label showing the current minimap mode.

**Files:**
- Modify: `src/ui/Minimap.cpp`
- Modify: `src/ui/Minimap.h`

- [ ] **Step 1: Handle MinimapLargeTerrain2 in the unit rendering loop**

In `Minimap::update()`, replace the mode check at lines 371-374:

```cpp
            if (mode != genie::Unit::MinimapUnit && mode != genie::Unit::MinimapBuilding &&
                mode != genie::Unit::MinimapLargeTerrain && mode != genie::Unit::MinimapLargeTerrain2) {
                continue;
            }
```

And add `MinimapLargeTerrain2` to the rendering block. After the `MinimapLargeTerrain` block at lines 393-397, add:

```cpp
            } else if (mode == genie::Unit::MinimapLargeTerrain2) {
                rectangleSprite.rect = ScreenRect(pos, Size(size, size));
                const genie::Color &color = colors[unit->data()->MinimapColor];
                rectangleSprite.fillColor = Drawable::Color(color.r, color.g, color.b);
                m_unitsTexture->draw(rectangleSprite);
            }
```

- [ ] **Step 2: Add mode label text member to Minimap.h**

Add after `MinimapMode m_mode` (line 67):

```cpp
    Drawable::Text::Ptr m_modeLabel;
```

- [ ] **Step 3: Create the mode label in init()**

In `Minimap::init()`, after creating `m_terrainTexture` (after line 203), add:

```cpp
    m_modeLabel = m_renderTarget->createText(Drawable::Text::Plain);
    m_modeLabel->pointSize = 9;
    m_modeLabel->color = Drawable::White;
    m_modeLabel->outlineColor = Drawable::Black;
```

- [ ] **Step 4: Draw the mode label in draw()**

In `Minimap::draw()`, after drawing the camera rect (before the closing brace, after line 431), add:

```cpp
    // Draw mode indicator text
    if (m_modeLabel) {
        switch (m_mode) {
        case MinimapMode::Normal:     m_modeLabel->string = "Normal"; break;
        case MinimapMode::Economic:   m_modeLabel->string = "Economic"; break;
        case MinimapMode::Diplomatic: m_modeLabel->string = "Diplomatic"; break;
        }
        m_modeLabel->position = ScreenPos(m_rect.x + 2, m_rect.y - 14);
        m_renderTarget->draw(m_modeLabel);
    }
```

- [ ] **Step 5: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Run the game, verify the minimap shows a mode label above it. Press F4 to cycle modes and confirm the label updates. Also verify that large terrain objects (farms, etc.) with MinimapLargeTerrain2 now show correctly.

- [ ] **Step 6: Commit**

```bash
cd /home/dima/Projects/freeaoe && git add src/ui/Minimap.cpp src/ui/Minimap.h && git commit -m "$(cat <<'EOF'
feat: minimap mode label and MinimapLargeTerrain2 support

Show current minimap mode name above the minimap. Handle
MinimapLargeTerrain2 (mode 5) the same as MinimapLargeTerrain.
EOF
)"
```

---

### Task 6: Minimap -- differentiate terrain colors in Economic mode

In Economic mode, the terrain itself should visually highlight farmland and fish differently. Farmland tiles and shallow water (which may contain fish) should use distinctive colors.

**Files:**
- Modify: `src/ui/Minimap.cpp`

- [ ] **Step 1: Add terrain color override for Economic mode**

In `Minimap::update()`, inside the terrain drawing loop (around line 306-309), replace the terrain color assignment:

```cpp
                const genie::Color &color = colors[terrain.Colors[0]];
                if (visibility == VisibilityMap::Explored) {
                    tileShape.fillColor = Drawable::Color(color.r/2, color.g/2, color.b/2);
                } else {
                    tileShape.fillColor = Drawable::Color(color.r, color.g, color.b);
                }
```

With mode-aware coloring:

```cpp
                const genie::Color &color = colors[terrain.Colors[0]];
                Drawable::Color tileColor(color.r, color.g, color.b);

                // In Economic mode, tint water and farmland tiles
                if (m_mode == MinimapMode::Economic) {
                    if (terrain.IsWater) {
                        // Shallow water where fish may spawn: brighter blue
                        tileColor = Drawable::Color(60, 100, 200);
                    }
                    // Farmland terrain ID = 7
                    if (tile.terrainId == 7) {
                        tileColor = Drawable::Color(200, 180, 60); // yellow for farms
                    }
                }

                if (visibility == VisibilityMap::Explored) {
                    tileShape.fillColor = Drawable::Color(tileColor.r/2, tileColor.g/2, tileColor.b/2);
                } else {
                    tileShape.fillColor = tileColor;
                }
```

- [ ] **Step 2: Add genie/dat/Terrain.h include if not present**

The include `genie/dat/Terrain.h` is already included via `DataManager.h`, but verify it provides `IsWater`. It does (checked: `genie::Terrain::IsWater` is `int8_t`).

- [ ] **Step 3: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Run the game in Economic mode (F4). Water tiles should appear brighter blue, farmland tiles should show yellow. Other modes should be unchanged.

- [ ] **Step 4: Commit**

```bash
cd /home/dima/Projects/freeaoe && git add src/ui/Minimap.cpp && git commit -m "$(cat <<'EOF'
feat: minimap Economic mode terrain color overrides

Water tiles show brighter blue and farmland shows yellow in Economic
mode to help players identify resource-rich areas at a glance.
EOF
)"
```
