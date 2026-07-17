# freeaoe Master Implementation Plan — Phase 2

> **For agentic workers:** Execute plans in order below. Each plan is a standalone file with step-by-step tasks. After completing a plan, mark it `[x]` and move to the next.

**Goal:** Implement remaining AoE2 features from the audit, bringing freeaoe from ~90% to ~95%+ playable.

**Execution:** Sequential — complete one plan fully, build, verify, commit, then move to next.

---

## Execution Order (easiest/highest-impact first)

### Tier 1: Quick Wins
- [x] **1.** [Fishing Economy](2026-07-14-fishing-economy.md) — fishing ships auto-seek next fish, drop off at Dock
- [x] **2.** [Trebuchet Transform](2026-07-14-trebuchet-transform.md) — timed pack/unpack with ActionTransform

### Tier 2: Core Features
- [x] **3.** [Save/Load Game](2026-07-14-save-load.md) — F5 quick save, F9 quick load, unit restoration
- [x] **4.** [Ambient Sounds](2026-07-14-ambient-sounds.md) — looping water/forest environmental audio

### Tier 3: Visual Polish
- [x] **5.** [Wall Segments](2026-07-14-wall-segments.md) — direction-specific sprites, corner pillars
- [x] **6.** [Auto-Scout](2026-07-14-auto-scout.md) — DE-style compass button, fog-of-war exploration

---

## Per-Plan Execution Protocol

1. Read the plan file
2. Execute all tasks in order (each task has steps with code)
3. Build: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
4. If build fails, fix and retry
5. Commit with descriptive message
6. Mark plan `[x]` in this file
7. Move to next plan

## Progress Tracking

Current plan: **DONE**
Plans completed: **6/6**
