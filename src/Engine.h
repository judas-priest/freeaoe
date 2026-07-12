/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2011  <copyright holder> <email>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "core/Types.h"
#include "mechanics/StateManager.h"

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"

#ifndef USE_SDL2
#ifndef USE_SDL2
#include <SFML/Graphics/Text.hpp>
#endif
#endif

#include <chrono>
#include <array>
#include <memory>
#include <string>
#include <vector>

class GameState;
class Map;
class MapRenderer;
class ActionPanel;
#ifndef USE_SDL2
class SfmlRenderTarget;
#endif
struct Dialog;
struct IconButton;
class Minimap;
struct NumberLabel;
class UiScreen;
class UnitInfoPanel;
class UnitsRenderer;

namespace genie {
class ScnFile;
}  // namespace genie

namespace genie {
class ScnFile;
}

#ifdef USE_SDL2
struct SdlWindow;
#else
namespace sf {
class RenderWindow;
}
#endif

struct MouseCursor;

class Engine
{
public:
    static const int s_numMessagesLines = 15;
    static const Time s_messageTimeout = 30000; // 30 seconds, I don't remember what it really is

    static inline int64_t currentTimeMs() {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();
    }

    Engine();
    virtual ~Engine();

    bool setup(const std::shared_ptr<genie::ScnFile> &scenario = nullptr);
    void start();

    void addMessage(const std::string &message);

private:
    void showStartScreen();
    void loadTopButtons();
    void loadUiOverlay();
    void drawUi();
    void drawEntities(const std::shared_ptr<Map> &map);
    bool updateCamera(const std::shared_ptr<GameState> &state);
    bool handleEvent(const input::Event &event, const std::shared_ptr<GameState> &state);
    bool handleKeyEvent(const input::Event &event, const std::shared_ptr<GameState> &state);
    bool handleMouseMove(const input::Event &event, const std::shared_ptr<GameState> &state);
    bool handleMousePress(const input::Event &event, const std::shared_ptr<GameState> &state);
    bool handleMouseRelease(const input::Event &event, const std::shared_ptr<GameState> &state);
    bool handleTouchEvent(const input::Event &event, const std::shared_ptr<GameState> &state);
    void showMenu();
    bool updateUi(const std::shared_ptr<GameState> &state);

#ifdef USE_SDL2
    std::unique_ptr<SdlWindow> m_sdlWindow;
    std::shared_ptr<IRenderTarget> renderTarget_;
#else
    std::shared_ptr<sf::RenderWindow> renderWindow_;
    std::shared_ptr<SfmlRenderTarget> renderTarget_;
#endif
    std::unique_ptr<Dialog> m_currentDialog;

    std::unique_ptr<UiScreen> m_mainScreen;
    std::unique_ptr<UnitsRenderer> m_unitsRenderer;

    StateManager state_manager_;

    Drawable::Text::Ptr m_resultOverlay;

    Drawable::Text::Ptr fps_label_;
    std::vector<std::unique_ptr<IconButton>> m_buttons;

    std::unique_ptr<NumberLabel> m_woodLabel;
    std::unique_ptr<NumberLabel> m_foodLabel;
    std::unique_ptr<NumberLabel> m_goldLabel;
    std::unique_ptr<NumberLabel> m_stoneLabel;
    std::unique_ptr<NumberLabel> m_populationLabel;

    std::unique_ptr<MouseCursor> m_mouseCursor;

    float m_cameraDeltaX = 0.f;
    float m_cameraDeltaY = 0.f;

    Time m_lastUpdate = 0u;

    std::unique_ptr<Minimap> m_minimap;
    std::unique_ptr<ActionPanel> m_actionPanel;
    std::unique_ptr<UnitInfoPanel> m_unitInfoPanel;
    std::unique_ptr<MapRenderer> m_mapRenderer;

    struct MessageLine {
        Drawable::Text::Ptr text;
        Time endTime = 0;
    };
    std::array<MessageLine, s_numMessagesLines> m_visibleText;

    Drawable::Image::Ptr m_uiOverlay;
    int m_uiOverlayOffset = 0;

    ScreenPos m_selectionStart;
    ScreenPos m_selectionCurr;
    ScreenRect m_selectionRect;
    bool m_selecting = false;

    struct TouchState {
        bool active = false;
        bool pinching = false;
        ScreenPos startPos;
        ScreenPos lastPos;
        int64_t startTime = 0;
        bool dragging = false;
        int64_t pendingTapTime = 0;
        ScreenPos pendingTapPos;
        bool hasPendingTap = false;
        bool suppressNextMouseRelease = false;
        static constexpr float DRAG_THRESHOLD = 25.f;
        static constexpr int64_t DOUBLE_TAP_MS = 500;
        static constexpr float DOUBLE_TAP_DIST = 80.f;
    } m_touchState;

    struct InputState {
        bool mouseDown = false;
        bool dragging = false;
        ScreenPos pressPos;
        ScreenPos lastMovePos;
        int64_t pressTime = 0;
        int64_t lastClickTime = 0;
        ScreenPos lastClickPos;
        static constexpr float DRAG_THRESHOLD = 25.f;
        static constexpr int64_t DOUBLE_CLICK_MS = 500;
        static constexpr float DOUBLE_CLICK_DIST = 80.f;
    } m_input;

    float m_gameAreaHeight = 800.f;
#ifdef USE_SDL2
    struct SDL_Texture *m_gameTexture = nullptr;
#endif
    Size m_baseViewportSize;
    float m_zoomLevel = 1.0f;
    static constexpr float ZOOM_MIN = 0.5f;
    static constexpr float ZOOM_MAX = 2.0f;
    static constexpr float PINCH_SENSITIVITY = 2.0f;
};

