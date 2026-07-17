# Plan: Victory/Defeat Screen Enhancement

**Date:** 2026-07-14
**Branch:** android-port
**Scope:** Engine.cpp only — enhance existing overlay, no new class

---

## Current State

`Engine.cpp` lines 376–406 already render a post-game overlay:
- Black semi-transparent fullscreen rect (alpha 180)
- `m_resultOverlay` — a centered `Drawable::Text::UI` with "You are victorious!" / "You have been defeated!"
- `m_statText` — reused for each stat row (Score, Units Killed, Units Lost, Buildings Razed, Techs Researched)
- A dimmed `"Tap Menu to exit"` hint at the bottom

This runs after `drawUi()` and before the SFML/SDL present call, so it sits on top of everything.

### What's missing
- No tappable buttons — user has to navigate away via the HUD menu button
- The stat block is left-aligned at `ws.width/2 - 100`, looks unpolished
- No "Continue Playing" option (post-defeat sandbox)
- No panel background — raw text on semi-transparent black

---

## What to Build

Keep the enhancement entirely inside `Engine.cpp` / `Engine.h`. No new files. Follow the same pattern as `Dialog`: `TextButton` objects stored as members, hit-tested in `handleEvent`.

### Visual layout (centered panel)

```
+------------------------------------+
|                                    |
|     You are victorious!            |  <- m_resultOverlay (large, centered)
|                                    |
|  Score              1 240          |
|  Units Killed          18          |
|  Units Lost             3          |
|  Buildings Razed        2          |
|  Techs Researched       5          |
|                                    |
|   [ Return to Menu ]               |
|   [ Continue Playing ]             |  <- single-player only
|                                    |
+------------------------------------+
```

Panel is drawn with a filled rect (same stone-brown as Dialog: `Drawable::Color(60, 50, 40, 220)`) with a 2 px border (`Drawable::Color(160, 140, 100, 255)`).

---

## Implementation

### Step 1 — Add members to Engine.h

```cpp
// Post-game overlay buttons
TextButton m_btnReturnToMenu;
TextButton m_btnContinuePlaying;
bool m_showContinueButton = false;  // set false for MP, true for SP
```

`TextButton` already has `rect`, `pressed`, `text`, `render(UiScreen*)`, `setRenderTarget()` — no changes needed to that class.

### Step 2 — Initialize in Engine::setup() (near m_resultOverlay init, ~line 1624)

```cpp
m_btnReturnToMenu.text = "Return to Menu";
m_btnContinuePlaying.text = "Continue Playing";
m_btnReturnToMenu.setRenderTarget(renderTarget_);
m_btnContinuePlaying.setRenderTarget(renderTarget_);
```

`m_showContinueButton` defaults to `true`; set it `false` when multiplayer is detected (hook into wherever multiplayer state is determined — currently always SP on Android, so always `true` for now).

### Step 3 — Replace the overlay drawing block (lines 376–406)

Replace the current block with:

```cpp
if (state->result != GameState::Result::Running) {
    const Size ws = renderTarget_->getSize();

    // Fullscreen dim
    renderTarget_->draw(ScreenRect(0, 0, ws.width, ws.height),
                        Drawable::Color(0, 0, 0, 180));

    // Panel geometry
    const int panelW = std::min(500, ws.width - 40);
    const int panelH = 320;
    const int panelX = (ws.width  - panelW) / 2;
    const int panelY = (ws.height - panelH) / 2;

    // Panel background
    renderTarget_->draw(ScreenRect(panelX, panelY, panelW, panelH),
                        Drawable::Color(60, 50, 40, 220));
    // Panel border (4 edge rects, 2 px thick)
    const Drawable::Color border(160, 140, 100, 255);
    renderTarget_->draw(ScreenRect(panelX,            panelY,            panelW, 2), border);
    renderTarget_->draw(ScreenRect(panelX,            panelY+panelH-2,   panelW, 2), border);
    renderTarget_->draw(ScreenRect(panelX,            panelY,            2, panelH), border);
    renderTarget_->draw(ScreenRect(panelX+panelW-2,   panelY,            2, panelH), border);

    // Title
    m_resultOverlay->position = ScreenPos(ws.width / 2, panelY + 36);
    renderTarget_->draw(m_resultOverlay);

    // Stats
    const Player::Ptr &human = state->humanPlayer();
    if (human) {
        const int statX     = panelX + 24;
        const int valueX    = panelX + panelW - 24;   // right-align values here
        float sy = panelY + 76;
        m_statText->color = Drawable::Color(200, 190, 150, 255);

        auto drawStat = [&](const std::string &label, int value) {
            // label left-aligned
            m_statText->string = label;
            m_statText->position = ScreenPos(statX, sy);
            renderTarget_->draw(m_statText);
            // value right-aligned (shift left by approx char width)
            m_statText->string = std::to_string(value);
            m_statText->position = ScreenPos(valueX - int(m_statText->string.size()) * 9, sy);
            renderTarget_->draw(m_statText);
            sy += 26;
        };

        drawStat("Score",             human->score());
        drawStat("Units Killed",      human->unitsKilled);
        drawStat("Units Lost",        human->unitsLost);
        drawStat("Buildings Razed",   human->buildingsRazed);
        drawStat("Techs Researched",  human->techsResearched);

        // Buttons — 60 px tall for touch targets
        const int btnW  = panelW - 48;
        const int btnH  = 52;
        const int btnX  = panelX + 24;
        int btnY = panelY + panelH - (m_showContinueButton ? 130 : 74);

        m_btnReturnToMenu.setRenderTarget(renderTarget_);
        m_btnReturnToMenu.rect = ScreenRect(btnX, btnY, btnW, btnH);
        m_btnReturnToMenu.render(this);

        if (m_showContinueButton) {
            btnY += btnH + 8;
            m_btnContinuePlaying.setRenderTarget(renderTarget_);
            m_btnContinuePlaying.rect = ScreenRect(btnX, btnY, btnW, btnH);
            m_btnContinuePlaying.render(this);
        }
    }
}
```

Note: `m_resultOverlay` already has `alignment = AlignCenter` and `pointSize = 25` — no change needed. The `m_statText` `pointSize` is 14 — fine as-is.

Right-aligning values with `int(str.size()) * 9` is a rough fixed-width estimate. This is acceptable for a monospace-ish font; refine if the font renders proportionally.

### Step 4 — Hit-test in handleEvent()

In `Engine::handleEvent()`, just before (or inside) the existing `if (m_currentDialog)` block, add:

```cpp
// Post-game overlay buttons
if (state && state->result != GameState::Result::Running) {
    ScreenPos pos;
    bool isRelease = false;

    if (event.type == input::Event::MouseButtonReleased) {
        pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isRelease = true;
    } else if (event.type == input::Event::TouchEnded) {
        pos = ScreenPos(event.touch.x, event.touch.y);
        isRelease = true;
    } else if (event.type == input::Event::MouseButtonPressed ||
               event.type == input::Event::TouchBegan) {
        // Track pressed state for visual feedback
        ScreenPos p = (event.type == input::Event::MouseButtonPressed)
            ? ScreenPos(event.mouseButton.x, event.mouseButton.y)
            : ScreenPos(event.touch.x, event.touch.y);
        m_btnReturnToMenu.pressed   = m_btnReturnToMenu.rect.contains(p);
        m_btnContinuePlaying.pressed = m_btnContinuePlaying.rect.contains(p);
        return true;  // consume — don't let clicks fall through to game
    }

    if (isRelease) {
        m_btnReturnToMenu.pressed    = false;
        m_btnContinuePlaying.pressed = false;

        if (m_btnReturnToMenu.rect.contains(pos)) {
            // Close the SDL window — main() will exit and Android Activity
            // will restart ScenarioBrowser on next launch via the OS back stack.
            // On desktop this quits the process (same as Quit in Dialog).
            m_sdlWindow->close();
            return true;
        }
        if (m_showContinueButton && m_btnContinuePlaying.rect.contains(pos)) {
            // Reset result so the game loop resumes; time continues from now.
            state->result = GameState::Result::Running;
            return true;
        }
    }
    return true;  // always consume input when overlay is visible
}
```

Place this block **before** the `if (m_currentDialog)` check so the dialog can't accidentally fire through the overlay.

The `state` parameter is already available in `handleEvent` signature: `Engine::handleEvent(const input::Event &event, const std::shared_ptr<GameState> &state)`.

### Step 5 — Make GameState::result writable for "Continue Playing"

`result` is already a public field (`GameState::Result result = Result::Running;`) — no change needed. Setting it back to `Running` from outside is fine.

---

## Android-specific considerations

- `btnH = 52` gives ~14 mm at 96 dpi and ~9 mm at 160 dpi (typical phone). Increase to 64 if testing shows it's too tight.
- `panelW = min(500, ws.width - 40)` ensures the panel doesn't overflow on a 1080 px wide phone in portrait or landscape.
- "Return to Menu" on Android closes the SDL window, which ends the Java `SDLActivity`. The OS will return the user to the previous Activity (ScenarioBrowser or launcher). This is the correct Android lifecycle behavior — no special code needed.
- Do NOT call `ScenarioBrowser::show()` from inside Engine; that path is SDL-main-thread only on desktop.

---

## SLP assets (optional polish, deferred)

`sat_tabs.slp` (50765) and `tml_bck.slp` (50763) exist in the interface DRS. They could provide a stone-panel background instead of the filled rect. Load with:

```cpp
auto bg = AssetManager::Inst()->getSlp(50763, AssetManager::ResourceType::Interface);
```

This is deferred — the filled rect approach is sufficient and has zero asset-load risk.

---

## Files to change

| File | Change |
|------|--------|
| `src/Engine.h` | Add `TextButton m_btnReturnToMenu`, `m_btnContinuePlaying`, `bool m_showContinueButton` |
| `src/Engine.cpp` | Replace lines 376–406 with panel + buttons; add hit-test block in `handleEvent` |

No other files need touching. `TextButton`, `Dialog`, `Player`, `GameState` are all used as-is.

---

## Test checklist

- [ ] Win condition triggers (DeclareVictory effect or last-enemy-eliminated): overlay appears, game pauses
- [ ] Lose condition (Defeat effect): overlay appears
- [ ] "Return to Menu" tap: window closes cleanly (logcat: no crash)
- [ ] "Continue Playing" tap: overlay disappears, game resumes, units still respond to commands
- [ ] Stats values match actual in-game events (kill a unit, verify Units Killed = 1)
- [ ] Panel fits on 1080x1920 (portrait) and 1920x1080 (landscape) without clipping
- [ ] Tapping the game area behind the overlay does NOT issue commands (input consumed)
