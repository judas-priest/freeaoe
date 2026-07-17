#pragma once

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"
#include "core/Types.h"

#include <memory>
#include <vector>
#include <map>
#include <string>

class GameState;

// Tech tree viewer — grid layout organized by building (rows) and age (columns)
class TechTreeScreen
{
public:
    TechTreeScreen(const std::shared_ptr<IRenderTarget> &renderTarget);

    void show(const std::shared_ptr<GameState> &state);
    void hide();
    bool isVisible() const { return m_visible; }

    void render();
    bool handleEvent(const input::Event &event);

private:
    std::shared_ptr<IRenderTarget> m_renderTarget;
    std::shared_ptr<GameState> m_state;
    bool m_visible = false;
    float m_scrollY = 0;
    float m_contentHeight = 0;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_itemText;
    Drawable::Text::Ptr m_tooltipText;

    // Individual tech/unit entry in the grid
    struct CellEntry {
        std::string name;
        int techId = -1;     // -1 for units
        int age = 0;         // 0=dark, 1=feudal, 2=castle, 3=imperial
        bool researched = false;
        bool available = false;
        bool isUnit = false;
        // Cost info for tooltip
        int costFood = 0;
        int costWood = 0;
        int costGold = 0;
        int costStone = 0;
        int researchTime = 0;
    };

    // A building row: building name + entries per age column
    struct BuildingRow {
        int16_t buildingId = -1;
        std::string buildingName;
        std::vector<CellEntry> entries[4]; // one vector per age
    };

    std::vector<BuildingRow> m_rows;

    // Tooltip state
    struct {
        bool active = false;
        float x = 0, y = 0;
        std::string text;
    } m_tooltip;

    // Mouse/touch position for hover detection and scroll
    float m_mouseX = 0, m_mouseY = 0;
    float m_lastTouchY = 0;

    // Rendered cell rectangles for hit-testing
    struct CellRect {
        ScreenRect rect;
        const CellEntry *entry = nullptr;
    };
    std::vector<CellRect> m_cellRects;

    void buildGrid();
};
