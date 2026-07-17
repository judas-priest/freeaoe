# Module 27: AI Script (.per) Loading — ALREADY IMPLEMENTED

> **Status: NO WORK NEEDED** — This module was included in the audit by mistake.

## What Was Found

Deep code verification revealed that .per AI script loading is **already fully implemented**:

1. **Parser exists**: `src/ai/ScriptLoader.h/cpp` — full bison/flex parser (`grammar.gen.tab.cpp`, `ScriptTokenizer.h`)
2. **Scenario .per loading**: `GameState.cpp:690-705` — loads embedded .per from scenario files via `genie::AiFile::perFile`
3. **Default script loading**: `GameState.cpp:903-924` — tries `{GamePath}/Ai/RandomGame.per` or `The Horde.per` for random map AI players
4. **Rule execution**: `AiScript::update()` evaluates loaded rules, `AiRule` has conditions/actions vectors
5. **BasicAI fallback**: `BasicAI` runs alongside scripts as hardcoded fallback behavior

## Architecture

```
GameState::setupRandomMap()
  -> creates AiPlayer
  -> ScriptLoader::parse(file) -> populates AiScript::rules
  -> BasicAI created as fallback
  -> AiPlayer::update() runs both AiScript rules AND BasicAI behaviors
```

## What Could Be Improved (Future)

- **More condition/action implementations**: Some `createCondition()` cases return nullptr with "unimplemented" warning
- **Script selection per civ**: Currently tries generic `RandomGame.per` — could load civ-specific scripts
- **Debug UI**: Show which rules are firing for AI debugging
