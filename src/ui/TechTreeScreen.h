#pragma once

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"
#include "core/Types.h"

#include <memory>
#include <vector>
#include <string>

class GameState;

// Simplified tech tree viewer — shows available/researched techs and units per age
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
    float m_scrollX = 0;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_itemText;

    struct TechEntry {
        std::string name;
        int age; // 0=dark, 1=feudal, 2=castle, 3=imperial
        bool researched;
        bool available;
    };
    std::vector<TechEntry> m_techs;

    void buildTechList();
};
