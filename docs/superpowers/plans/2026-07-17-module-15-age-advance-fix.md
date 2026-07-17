# Module 15: Age Advancement Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix age advancement so that researching Feudal/Castle/Imperial Age at a TC triggers the full implicit tech chain (same as `setAge()`), not just the research effect.

**Architecture:** In `Player::applyResearch()`, detect when the researched tech sets `CurrentAge` resource, and call `setAge()` which applies the `FeudalAgeTechID`/`CastleAgeTechID`/`ImperialAgeTechID` tech-tree effect and runs the implicit research loop.

**Tech Stack:** C++, Player::applyResearch, genie::ResourceType

**Verified APIs:**
- `Player::applyResearch(int researchId)` at line 66 of Player.cpp
- `Player::setAge(Age age)` at line 192 of Player.cpp
- `Player::applyTechEffect(int effectId)` — guards with `m_activeTechs.count(effectId)`, prevents double-apply
- `genie::ResourceType::CurrentAge = 6`
- `m_resourcesAvailable` is `ResourceMap` (map type) — accessed by `[]` operator with ResourceType key
- `Player::Age` enum: `DarkAge`, `FeudalAge`, `CastleAge`, `ImperialAge`

---

### Task 1: Bridge applyResearch to setAge

**Files:**
- Modify: `src/mechanics/Player.cpp`

**Context:** `Player::applyResearch(int researchId)` (line 66) calls `applyTechEffect(effectId)` which processes all `EffectCommand`s including `ResourceModifier` that sets `CurrentAge`. But it does NOT call `setAge()`, which applies the civ-specific `FeudalAgeTechID`/`CastleAgeTechID`/`ImperialAgeTechID` tech-tree effect.

`setAge(Age age)` (line 192) does:
1. Sets `m_resourcesAvailable[CurrentAge] = age`
2. Looks up `civilization.startingResource(FeudalAgeTechID)` etc.
3. Calls `applyTechEffect()` with that effect
4. Runs implicit research loop
5. Calls `updateAvailableTechs()`

The fix: after `applyTechEffect()` in `applyResearch()`, check if `CurrentAge` changed and call `setAge()` for the new age.

- [ ] **Step 1: Detect age change in applyResearch**

In `Player::applyResearch()`, after the call to `applyTechEffect(tech.EffectID)`, add:

```cpp
// Check if this research advanced the age — if so, apply full age-up chain
int newAge = static_cast<int>(m_resourcesAvailable[genie::ResourceType::CurrentAge]);
if (newAge != previousAge) {
    // setAge applies the civ-specific tech tree effect (FeudalAgeTechID etc.)
    // which enables/disables units and buildings per-civ
    setAge(static_cast<Age>(newAge));
}
```

And before the `applyTechEffect()` call, capture the previous age:

```cpp
int previousAge = static_cast<int>(m_resourcesAvailable[genie::ResourceType::CurrentAge]);
```

- [ ] **Step 2: Guard against double-application in setAge**

`setAge()` calls `applyTechEffect()` which checks `m_activeTechs` — if the effect ID is already active, it returns early. This prevents double-application. However, `setAge()` also sets `m_resourcesAvailable[CurrentAge]` again, which is harmless (same value).

Verify that `setAge()` won't re-apply effects that `applyResearch()` already applied. The guard in `applyTechEffect()`:
```cpp
if (m_activeTechs.count(effectId)) return;
```
ensures no double-application.

No additional changes needed — the existing guard is sufficient.

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

Test: start a game, research Feudal Age at TC. After completion, check that new buildings (Blacksmith, Market, etc.) appear in the build menu. Previously they might have been missing if the civ's TechTreeID effect had additional `EnableUnit` commands beyond what the age-up tech itself contains.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Player.cpp
git commit -m "fix: age advancement calls setAge() to apply full civ tech-tree chain"
```
