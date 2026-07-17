# Plan: Trade Gold Formula Fix

**Goal:** Replace the linear `goldEarned = tradeDist / 10.f` formula in `ActionTrade.cpp` line 43 with the correct AoE2 formula: `gold = 0.46 * d * (d / mapSize + 0.3)`, where `d` is the Euclidean tile distance between the home market and the foreign market, and `mapSize` is the map size in tiles.

**Status:** Not started

---

## Background

Current code (`ActionTrade.cpp` lines 41–43):
```cpp
float tradeDist = m_homePos.distance(market->position());
float goldEarned = tradeDist / 10.f;
```

The real AoE2 formula (from the community wiki) is:
```
gold = 0.46 * distance_tiles * (distance_tiles / map_size + 0.3)
```

where `distance_tiles` is the Euclidean distance in tiles between the two markets, and `map_size` is the map dimension in tiles (e.g., 120 for standard). The formula produces roughly 30 gold for adjacent markets on a tiny map and up to ~200 for max-distance trade on a large map.

---

## Change: `src/actions/ActionTrade.cpp`

### Add map size access

`ActionTrade` needs access to the map size. The unit already has a `map()` method returning `MapPtr`, and `Map` has `columnCount()` / `rowCount()`.

### Current code at lines 41–49:

```cpp
if (!m_goingToTarget) {
    // Arrived back home — deposit gold
    // Gold earned = distance between markets / 10
    float tradeDist = m_homePos.distance(market->position());
    float goldEarned = tradeDist / 10.f;

    auto owner = cart->player().lock();
    if (owner) {
        owner->setAvailableResource(genie::ResourceType::GoldStorage,
            owner->resourcesAvailable(genie::ResourceType::GoldStorage) + goldEarned);
    }
}
```

### Replacement:

```cpp
if (!m_goingToTarget) {
    // Arrived back home — deposit gold using AoE2 formula:
    //   gold = 0.46 * d_tiles * (d_tiles / mapSize + 0.3)
    // where d_tiles is Euclidean distance between markets in tiles.
    const float pixelsPerTile = 48.f; // Constants::TILE_SIZE
    const float tradeDist = m_homePos.distance(market->position());
    const float distTiles = tradeDist / pixelsPerTile;

    // Map size: use the larger of width/height in tiles
    float mapSize = 120.f; // default standard map
    if (cart->map()) {
        mapSize = float(std::max(cart->map()->columnCount(), cart->map()->rowCount()));
    }

    const float goldEarned = 0.46f * distTiles * (distTiles / mapSize + 0.3f);

    auto owner = cart->player().lock();
    if (owner) {
        owner->setAvailableResource(genie::ResourceType::GoldStorage,
            owner->resourcesAvailable(genie::ResourceType::GoldStorage) + goldEarned);
        DBG << "Trade: dist=" << distTiles << "tiles, mapSize=" << mapSize
            << ", gold=" << goldEarned;
    }
}
```

---

## Required includes

`ActionTrade.cpp` already includes `ActionMove.h` and `mechanics/Player.h`. The `map()` call on the cart requires `genie/dat/Unit.h` and the map headers.

Add if not present:

```cpp
#include "mechanics/Map.h"
#include <algorithm>
```

`cart->map()` returns the `MapPtr` stored in `Entity::m_map` and is accessible via `Entity::map()` (public, inherited by Unit).

---

## Verification

| Distance (tiles) | Map size | Expected gold | Old formula (d/10) |
|-----------------|----------|---------------|---------------------|
| 10              | 120      | 0.46 * 10 * (10/120 + 0.3) = ~1.77 | 0.48 |
| 50              | 120      | 0.46 * 50 * (50/120 + 0.3) = ~18.5 | 2.4 |
| 80              | 120      | 0.46 * 80 * (80/120 + 0.3) = ~35.5 | 3.84 |
| 100             | 120      | 0.46 * 100 * (100/120 + 0.3) = ~52.2 | 4.8 |

The old formula was in pixel units (distance ÷ 10), giving absurdly small amounts. The new formula uses tile units and produces amounts matching real AoE2.

---

## Testing

1. Create a game with two markets for different players.
2. Send a trade cart from one market to another.
3. When the cart returns, verify the gold received matches approximately the formula output.
4. Test at different market separations to confirm the quadratic scaling.
