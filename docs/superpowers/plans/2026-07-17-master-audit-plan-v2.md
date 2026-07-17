# freeaoe Full Audit — Master Plan Index v2

> Generated 2026-07-17 from comprehensive 4-agent codebase audit. Continues from modules 01-10 (already implemented).

## Modules 11-20: Remaining Gaps

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 11 | **Conquest Victory** | Small | Critical | `module-11-conquest-victory.md` |
| 12 | **AI Farm Economy** | Medium | Critical | `module-12-ai-farms.md` |
| 13 | **Map Resources (boar, wolves, fish, relics)** | Medium | Critical | `module-13-map-resources.md` |
| 14 | **Patrol & Guard Combat** | Medium | High | `module-14-patrol-guard-combat.md` |
| 15 | **Age Advancement Fix** | Small | High | `module-15-age-advance-fix.md` |
| 16 | **Save/Load Completeness** | Large | High | `module-16-save-load-complete.md` |
| 17 | **Game Setup Screen** | Large | Medium | `module-17-game-setup-screen.md` |
| 18 | **Map Terrain & Elevation** | Medium | Medium | `module-18-map-terrain.md` |
| 19 | **AI Buildings (Castle/Blacksmith/University)** | Medium | Medium | `module-19-ai-buildings.md` |
| 20 | **Tribute Tax & Data-Driven Rates** | Small | Low | `module-20-tribute-tax-fix.md` |

## What Was Already Done (Modules 01-10)

See `2026-07-17-master-audit-plan.md` for:
- Module 01: Ungarrison (done)
- Module 02: Stances (done)
- Module 03: Market UI (done)
- Module 04: Relic Deposit (done)
- Module 05: Formations (done)
- Module 06: Building Visuals
- Module 07: Diplomacy UI
- Module 08: Save/Load UI
- Module 09: Tree Visuals
- Module 10: Naval (done)

## Not Planned (too large / separate project)

- Scenario Editor (full rewrite needed)
- Multiplayer matchmaking / NAT traversal
- Full graphical tech tree (AoE2 SLP-based)
- Post-game statistics timeline/graphs
- Configurable hotkey system
- .dat-driven RandomMap scripts (replaces entire generator)
- Campaign save state / unlock tracking
- BIK video playback
- Group pathfinding / flow-field
- Additional map types beyond 5

## Quick Wins

1. **Module 11** — ~20 lines in GameState::update() and ScenarioController
2. **Module 15** — ~5 lines in Player::applyResearch()
3. **Module 20** — ~10 lines in GameState::executeCommands()
