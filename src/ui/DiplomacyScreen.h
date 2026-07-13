#pragma once

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"
#include "core/Types.h"

#include <memory>
#include <vector>
#include <string>

class GameState;

class DiplomacyScreen
{
public:
    DiplomacyScreen(const std::shared_ptr<IRenderTarget> &renderTarget);

    void show(const std::shared_ptr<GameState> &state);
    void hide();
    bool isVisible() const { return m_visible; }

    void render();
    bool handleEvent(const input::Event &event);

private:
    std::shared_ptr<IRenderTarget> m_renderTarget;
    std::shared_ptr<GameState> m_state;
    bool m_visible = false;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_itemText;
};
