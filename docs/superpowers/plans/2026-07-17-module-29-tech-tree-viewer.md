# Module 29: Tech Tree Viewer Enhancement Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enhance the tech tree viewer to show a visual tree layout organized by age and building, with available/researched/unavailable states.

**Architecture:** Extend existing `TechTreeScreen` to render a grid-based layout. Columns = ages (Dark/Feudal/Castle/Imperial). Rows = buildings. Each cell shows available techs/units from that building in that age, color-coded by research status. Uses existing genie tech data for layout.

**Tech Stack:** C++20, SDL2 rendering, existing TechTreeScreen, genie data

---

## Background

- `TechTreeScreen` exists (`src/ui/TechTreeScreen.h`) with basic rendering
- AoE2 tech tree: 4 age columns, rows per building, icons for each tech/unit
- Tech data includes `ResearchLocation` (building ID) and implicit age requirements
- `Player::m_researchedTechs` tracks what's been researched
- Civ restrictions: `tech.Civ != -1 && tech.Civ != m_civId` filters unavailable techs

## Key Files

- `src/ui/TechTreeScreen.h` — Existing tech tree screen
- `src/ui/TechTreeScreen.cpp` — Rendering
- `src/mechanics/Civilization.h` — `researchAvailableAt()`, tech data
- `src/mechanics/Player.h` — `m_researchedTechs`

---

### Task 1: Build tech tree data model

**Files:**
- Modify: `src/ui/TechTreeScreen.h`
- Modify: `src/ui/TechTreeScreen.cpp`

- [ ] **Step 1: Create data model for tree layout**

In `TechTreeScreen.h`, add structures to organize techs by building and age:

```cpp
struct TechNode {
    int techId;
    std::string name;
    int iconId;
    int age; // 0-3 (dark/feudal/castle/imperial)
    enum Status { Unavailable, Available, Researched };
    Status status;
};

struct BuildingRow {
    int buildingId;
    std::string buildingName;
    int buildingIconId;
    std::vector<std::vector<TechNode>> techsByAge; // [4] ages, each has list of techs
};

std::vector<BuildingRow> m_treeData;
```

- [ ] **Step 2: Populate tree data from genie techs**

In `TechTreeScreen.cpp`, populate the data model:

```cpp
void TechTreeScreen::buildTreeData(const Player &player)
{
    m_treeData.clear();
    std::map<int, BuildingRow> byBuilding;

    const auto &allTechs = DataManager::Inst().allTechs();
    for (size_t i = 0; i < allTechs.size(); i++) {
        const genie::Tech &tech = allTechs[i];
        if (tech.ResearchLocation <= 0) continue; // Skip implicit techs
        if (tech.Civ != -1 && tech.Civ != player.civilization.id()) continue;

        TechNode node;
        node.techId = i;
        node.name = tech.Name;
        node.iconId = tech.IconID;
        node.age = determineAge(tech); // Based on required techs
        node.status = player.hasResearched(i) ? TechNode::Researched
                    : player.canResearch(i) ? TechNode::Available
                    : TechNode::Unavailable;

        byBuilding[tech.ResearchLocation].techsByAge[node.age].push_back(node);
    }

    // Also add trainable units per building
    // ...

    for (auto &[buildingId, row] : byBuilding) {
        row.buildingId = buildingId;
        row.buildingName = player.civilization.unitData(buildingId).Name;
        m_treeData.push_back(std::move(row));
    }
}
```

- [ ] **Step 3: Build**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/ui/TechTreeScreen.h src/ui/TechTreeScreen.cpp
git commit -m "feat: tech tree data model organized by building and age"
```

---

### Task 2: Render tech tree grid

**Files:**
- Modify: `src/ui/TechTreeScreen.cpp`

- [ ] **Step 1: Implement grid rendering**

Render the tree as a scrollable grid:

```cpp
void TechTreeScreen::render(SDL_Renderer *renderer)
{
    // Background
    SDL_SetRenderDrawColor(renderer, 30, 20, 10, 255);
    SDL_RenderClear(renderer);

    // Age column headers
    const char *ageNames[] = {"Dark Age", "Feudal Age", "Castle Age", "Imperial Age"};
    const int colWidth = m_screenWidth / 4;
    const int rowHeight = 50;
    const int headerHeight = 40;
    const int iconSize = 32;

    for (int a = 0; a < 4; a++) {
        // Draw age header
        renderText(ageNames[a], a * colWidth + colWidth / 2, 10, true);
        // Draw column separator
        SDL_SetRenderDrawColor(renderer, 100, 80, 60, 255);
        SDL_RenderDrawLine(renderer, (a + 1) * colWidth, 0, (a + 1) * colWidth, m_screenHeight);
    }

    // Building rows
    int y = headerHeight - m_scrollY;
    for (const auto &row : m_treeData) {
        // Building name on left
        renderText(row.buildingName, 5, y + rowHeight / 2, false);

        // Tech icons per age
        for (int a = 0; a < 4; a++) {
            int x = a * colWidth + 10;
            for (const auto &node : row.techsByAge[a]) {
                // Color by status
                SDL_Color color;
                switch (node.status) {
                case TechNode::Researched: color = {0, 200, 0, 255}; break;
                case TechNode::Available: color = {200, 200, 200, 255}; break;
                case TechNode::Unavailable: color = {80, 80, 80, 128}; break;
                }

                // Draw icon background
                SDL_Rect iconRect = {x, y + 5, iconSize, iconSize};
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &iconRect);

                // Draw tech icon (from icon atlas)
                renderIcon(node.iconId, iconRect);

                x += iconSize + 4;
            }
        }

        y += rowHeight;
    }
}
```

- [ ] **Step 2: Add scrolling**

Handle mouse wheel / touch drag for vertical scrolling:

```cpp
void TechTreeScreen::handleScroll(int deltaY)
{
    m_scrollY -= deltaY * 20;
    m_scrollY = std::max(0, std::min(m_scrollY, m_maxScrollY));
}
```

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

Open tech tree in-game. Verify grid layout with age columns and building rows. Verify color coding (green = researched, white = available, grey = unavailable).

- [ ] **Step 4: Commit**

```bash
git add src/ui/TechTreeScreen.cpp
git commit -m "feat: visual tech tree grid with age columns and status colors"
```

---

### Task 3: Add tooltip on hover

**Files:**
- Modify: `src/ui/TechTreeScreen.cpp`

- [ ] **Step 1: Track hovered tech node**

On mouse move, find which tech icon is under cursor:

```cpp
void TechTreeScreen::handleMouseMove(int mx, int my)
{
    m_hoveredNode = nullptr;
    // Hit test against rendered icon positions
    // (store icon rects during render, or recalculate from grid position)
}
```

- [ ] **Step 2: Render tooltip**

If a node is hovered, draw a tooltip box with:
- Tech name
- Cost (food/wood/gold/stone)
- Research time
- Description (from language strings)

```cpp
if (m_hoveredNode) {
    const genie::Tech &tech = DataManager::Inst().getTech(m_hoveredNode->techId);
    std::string tooltip = m_hoveredNode->name + "\n";
    // Add cost lines
    // Add description
    renderTooltip(tooltip, mouseX, mouseY);
}
```

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/ui/TechTreeScreen.cpp
git commit -m "feat: tech tree tooltip showing cost and description"
```
