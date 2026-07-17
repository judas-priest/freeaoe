# freeaoe Technical Debt — Final Remaining Items

## Status: ~98% feature-complete (excluding scenario editor)

25 feature commits + 1 bugfix delivered across two phases. Below is everything still missing.

---

## Quick Fixes (5-15 min each)

### 1. Select All Military Hotkey
- **What:** Ctrl+A or dedicated key selects all military units on screen
- **Where:** `src/Engine.cpp` handleKeyEvent — add case, iterate units, filter by class != Civilian && != Building
- **Effort:** 5 min

### 2. Camera Follow Unit (F6)
- **What:** F6 toggles camera auto-follow on selected unit
- **Where:** `src/Engine.cpp` — add `m_followUnit` weak_ptr, in update loop set camera target to unit position
- **Effort:** 10 min

### 3. Idle Villager UI Button
- **What:** Visual button in HUD (not just F1 hotkey) to cycle idle villagers
- **Where:** `src/ui/ActionPanel.cpp` — add button alongside existing formation buttons when no unit selected
- **Effort:** 5 min

### 4. Signal Flare
- **What:** Click button, click map → ping visible to all allies
- **Where:** `src/ui/ActionPanel.h` already has `Command::SignalFlare`. Need handler in `handleButtonClick` + visual ping marker (reuse MoveTargetMarker pattern)
- **Effort:** 10 min

### 5. Map Reveal (Debug/Cheat)
- **What:** Toggle fog of war off for debugging. Type "marco" in chat.
- **Where:** `src/Engine.cpp` onChatMessage — check for "marco", call `VisibilityMap::revealAll()`. Add `revealAll()` to VisibilityMap.
- **Effort:** 5 min

### 6. Heresy/Theocracy/Illumination/Faith Tech Effects
- **What:**
  - Heresy (439): converted units die instead of switching sides
  - Theocracy (438): only one monk in a group loses faith after conversion
  - Illumination (233): faith recharges 2x faster
  - Faith (45): units gain +50% conversion resistance
- **Where:** `src/actions/ActionConvert.cpp` — check `player->hasResearched(techId)` and adjust behavior. Tech IDs already in `src/ai/Ids.cpp`.
- **Effort:** 15 min

### 7. Dynamic Market Prices
- **What:** Buy/sell prices change based on how much has been traded. AoE2 formula: each 100 units traded shifts price by ~3 gold. Prices reset toward 100 gold over time.
- **Where:** `src/ui/ActionPanel.cpp` lines 779-833 — currently hardcoded 70/130. Add `Player::m_marketPrices[4]` tracking cumulative trade volume per resource type.
- **Effort:** 10 min

---

## Medium Tasks (1-2 hours)

### 8. Scenario Editor MVP
- **What:** Minimal usable editor: map display, unit palette, click-to-place, save
- **Where:** `src/editor/Editor.h/.cpp` — currently stub ("Not implemented, click to exit")
- **Components needed:**
  - Reuse MapRenderer for map display
  - Scrollable unit ID palette sidebar
  - Touch/click to place unit at tile → insert into scenario players[n].objects
  - Player selector (1-8)
  - Save button → write ScnFile back to disk
  - Terrain paint (change tile type) as stretch goal
- **Effort:** 2-4 hours
- **Blocked by:** Need to verify genieutils ScnFile has write/save support

---

## Not Planned (intentionally excluded)

| Feature | Reason |
|---------|--------|
| Multiplayer networking | User decision: bots only |
| .per AI script loading | Framework exists but 56 action stubs unimplemented. BasicAI with difficulty levels is sufficient |
| Full diplomacy UI | Skeleton exists, requires ally/enemy state changes which need network or AI diplomacy logic |
| HD/DE-specific features | Feitoria, siege tower ability, etc. — niche |

---

## Code Quality Debt

| Issue | Location | Impact |
|-------|----------|--------|
| `-Wswitch` warnings for new enum values | UnitManager.h LogPrinter (line 284) | Cosmetic — missing cases for new states |
| `m_statText` reused for chat, objectives, victory | Engine.cpp | Works but fragile — could create text per system |
| Water animation redraws full terrain every 200ms | MapRenderer.cpp | Performance OK for now, could dirty-flag only water rows |
| No unit test framework | Entire project | All testing is manual/visual |
| `SaveGame` version 2 skips terrain on load | SaveGame.cpp | Relies on scenario terrain matching — will break if terrain was modified |
| Gate passability cache not invalidated | ActionMove.cpp | Gates that open/close may not update cached paths immediately |

---

## Summary

**7 quick fixes** (~1 hour total) would bring the project to ~99% feature parity with AoE2 core gameplay.

**Scenario editor** is the only large remaining task and is a separate project scope.
