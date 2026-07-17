# Plan: Trigger System Gaps (AISignal, HD_Chance, AIScriptGoal)

**Goal:** Implement the three remaining trigger primitives:
1. **AISignal condition** — fires when a specific AI signal is raised. Requires `m_pendingAISignals` set in `ScenarioController` and a handler for `EventManager::aiSignal`.
2. **HD_Chance condition** — one-line probabilistic condition: fires with probability `amount`%.
3. **AIScriptGoal effect** — sets/clears a signal from the pending set, unblocking AISignal conditions.

**Status:** Not started

---

## Context

`ScenarioController::setScenario` (lines 68–93 in `ScenarioController.cpp`) only loads triggers whose conditions are in a known-implemented switch. The three gaps are:

- `genie::TriggerCondition::AISignal` — not in the switch (falls to default)
- `genie::TriggerCondition::HD_Chance` — not in the switch
- `genie::TriggerEffect::AIScriptGoal` — not in the implemented effects switch (lines 111–142)

---

## Step 1: AISignal condition

### Add `m_pendingAISignals` to `ScenarioController.h`

```cpp
#include <unordered_set>

// In private section:
std::unordered_set<int> m_pendingAISignals;
```

### Register for `AiSignal` events

In `ScenarioController::ScenarioController()` or `setScenario()`:

```cpp
EventManager::registerListener(this, EventManager::AiSignal);
```

### Override `onAiSignal` in `ScenarioController.h`

```cpp
void onAiSignal(Player *player, int signalId) override;
```

### Implement in `ScenarioController.cpp`

```cpp
void ScenarioController::onAiSignal(Player * /*player*/, int signalId)
{
    m_pendingAISignals.insert(signalId);
    DBG << "AI signal received:" << signalId;
}
```

### Add `AISignal` to the implemented-conditions switch

In `setScenario()`, add to the `case` list (line 68–88):

```cpp
case genie::TriggerCondition::AISignal:
case genie::TriggerCondition::HD_Chance:
    isImplemented = true;
    break;
```

### Evaluate AISignal condition in `update()`

In `ScenarioController::update()`, in the condition-checking loop, add a case for `AISignal`:

Find where individual conditions are decremented/checked. The current pattern appears to be that each `Condition::amountRequired` is decremented by event handlers or checked in the update loop.

Add to the condition evaluation (in the `update` loop where conditions are checked):

```cpp
case genie::TriggerCondition::AISignal: {
    const int signalId = condition.data.aiSignal;
    if (m_pendingAISignals.count(signalId) > 0) {
        condition.amountRequired = 0; // satisfied
    }
    break;
}
```

---

## Step 2: HD_Chance condition

HD_Chance fires with probability = `condition.data.amount` percent. It re-evaluates each time the trigger is checked.

In the condition evaluation loop, add:

```cpp
case genie::TriggerCondition::HD_Chance: {
    // amount is the percentage probability (0–100)
    const int probability = int(condition.data.amount);
    if ((rand() % 100) < probability) {
        condition.amountRequired = 0; // this roll satisfied it
    }
    // If not satisfied, amountRequired stays > 0 and it will re-roll next update
    break;
}
```

**Important:** Since HD_Chance rolls on every update tick, it will eventually succeed on any trigger that loops. For one-shot triggers, the trigger is disabled after firing so there is no issue.

---

## Step 3: AIScriptGoal effect

`genie::TriggerEffect::AIScriptGoal` sets a "goal" (signal) for AI players. In the context of trigger conditions, it is used to clear or set AI signals that unblock AISignal conditions.

In `ScenarioController::handleTriggerEffect`, add to the effect switch:

### Add to the implemented-effects switch in `setScenario()`:

```cpp
case genie::TriggerEffect::AIScriptGoal:
    break; // now implemented
```

Remove from the `default:` missing-effects warning path.

### Implement in `handleTriggerEffect`:

```cpp
case genie::TriggerEffect::AIScriptGoal: {
    // effect.aiGoal is the signal ID to set
    const int goalId = effect.aiGoal;
    if (goalId >= 0) {
        m_pendingAISignals.insert(goalId);
        DBG << "AIScriptGoal: set signal" << goalId;
        // Also fire the event so other listeners can react
        if (m_gameState) {
            // Find the source player
            Player *sourcePlayer = nullptr;
            if (effect.sourcePlayer >= 0) {
                sourcePlayer = m_gameState->getPlayer(effect.sourcePlayer);
            }
            EventManager::aiSignal(sourcePlayer, goalId);
        }
    }
    break;
}
```

---

## Step 4: Clear signals after they are consumed

AISignal conditions should be one-shot: once a trigger fires because of a signal, the signal should be cleared so the same trigger does not re-fire (unless the trigger is looping).

In `ScenarioController::update()`, after a trigger fires its effects:

```cpp
// After effects are executed:
if (!trigger.looping) {
    // Clear any AI signals that satisfied conditions in this trigger
    for (const Condition &cond : trigger.conditions) {
        if (cond.data.type == genie::TriggerCondition::AISignal) {
            m_pendingAISignals.erase(cond.data.aiSignal);
        }
    }
}
```

---

## Where the condition evaluation loop is

The update loop in `ScenarioController::update()` checks `trigger.isSatisfied()`, which checks that all `condition.amountRequired > 0`. The individual condition decrement/evaluation happens via event callbacks (e.g., `onUnitDying` decrements `DestroyObject` conditions).

For `AISignal` and `HD_Chance`, the check must happen in the `update()` loop itself since there is no dedicated event. Add a pass before the `isSatisfied()` check:

```cpp
bool ScenarioController::update(Time time)
{
    // ...
    for (Trigger &trigger : m_triggers) {
        if (!trigger.enabled) continue;

        // Evaluate time-based and poll-based conditions
        for (Condition &condition : trigger.conditions) {
            switch (condition.data.type) {
            case genie::TriggerCondition::Timer:
                // existing timer logic
                break;
            case genie::TriggerCondition::AISignal:
                if (m_pendingAISignals.count(condition.data.aiSignal) > 0) {
                    condition.amountRequired = 0;
                }
                break;
            case genie::TriggerCondition::HD_Chance:
                if (condition.amountRequired > 0) {
                    const int probability = int(condition.data.amount);
                    if ((rand() % 100) < probability) {
                        condition.amountRequired = 0;
                    }
                }
                break;
            default:
                break;
            }
        }

        if (trigger.isSatisfied()) {
            // Fire effects
            for (const genie::TriggerEffect &effect : trigger.effects) {
                handleTriggerEffect(effect);
            }
            // Reset or disable
            if (trigger.looping) {
                for (Condition &c : trigger.conditions) {
                    c.amountRequired = c.originalAmount;
                }
            } else {
                trigger.enabled = false;
            }
        }
    }
    // ...
}
```

---

## Impact

With these three additions:
- `AISignal` conditions work: one trigger can set a signal via `AIScriptGoal`, which unblocks another trigger that has `AISignal` as its condition.
- `HD_Chance` works: random probability conditions in HD scenarios will evaluate correctly.
- The trigger count should increase from 48/49 to near 100% for standard AoE2 scenarios.

---

## Testing

1. Create a test scenario with:
   - Trigger A: no condition, effect = `AIScriptGoal(signal=1)`, starts enabled.
   - Trigger B: condition = `AISignal(1)`, effect = `SendChat("Signal received!")`.
2. Load scenario — Trigger A should fire immediately, setting signal 1, which unblocks Trigger B, which should display the chat message.
3. Test HD_Chance: trigger with HD_Chance(50) — should fire approximately 50% of the time the trigger is checked.
