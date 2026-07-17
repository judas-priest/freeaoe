# freeaoe Master Implementation Plan

> **For agentic workers:** Execute plans in order below. Each plan is a standalone file with step-by-step tasks. After completing a plan, mark it `[x]` and move to the next.

**Goal:** Implement all missing AoE2 features identified in the full audit, bringing freeaoe from ~70% to ~95% playable.

**Execution:** Sequential — complete one plan fully, build, verify, commit, then move to next.

---

## Execution Order (easiest/highest-impact first)

### Tier 1: Quick Wins (trivial changes)
- [x] **1.** [Mine Depletion](2026-07-14-mine-depletion.md) — 1-line sprite swap when mine exhausted
- [x] **2.** [Trade Formula](2026-07-14-trade-formula.md) — fix gold formula from linear to quadratic
- [x] **3.** [Trigger Gaps](2026-07-14-trigger-gaps.md) — AISignal + HD_Chance + AIScriptGoal (3 case statements)
- [x] **4.** [Edge Panning](2026-07-14-edge-panning.md) — mouse edge camera scroll

### Tier 2: Playability Blockers
- [x] **5.** [Command Queue](2026-07-14-command-queue.md) — shift+click action queue
- [x] **6.** [Rally Points](2026-07-14-rally-points.md) — building rally points with auto-gather
- [x] **7.** [Victory Screen](2026-07-14-victory-screen.md) — victory/defeat panel with Return to Menu
- [x] **8.** [AI Difficulty](2026-07-14-ai-difficulty.md) — 5 difficulty levels

### Tier 3: Combat and Economy
- [x] **9.** [Attack Move](2026-07-14-attack-move.md) — one-shot attack-move action
- [x] **10.** [Conversion Mechanics](2026-07-14-conversion-mechanics.md) — faith + probability ramp
- [x] **11.** [Relic Drop](2026-07-14-relic-drop.md) — relic drops on monk death
- [x] **12.** [Town Bell](2026-07-14-town-bell.md) — ring/abort town bell

### Tier 4: Visual Polish
- [x] **13.** [Building Damage](2026-07-14-building-damage.md) — fire overlay + rubble
- [x] **14.** [Water Animation](2026-07-14-water-animation.md) — animated water tiles
- [x] **15.** [Minimap Modes](2026-07-14-minimap-modes.md) — Normal/Economic/Diplomatic toggle

### Tier 5: UI Features
- [x] **16.** [Chat UI](2026-07-14-chat-ui.md) — text input + taunts
- [x] **17.** [Objectives Panel](2026-07-14-objectives-panel.md) — trigger-based objectives HUD
- [x] **18.** [Campaign Progression](2026-07-14-campaign-progression.md) — next scenario after victory

### Tier 6: Complex Mechanics
- [x] **19.** [Gate Mechanics](2026-07-14-gate-mechanics.md) — open/close, lock, pathfinding integration

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
Plans completed: **19/19**
