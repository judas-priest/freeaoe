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


#include "Engine.h"

#include "core/Logger.h"
#include "core/ResourceMap.h"
#include "mechanics/GameState.h"
#include "mechanics/Map.h"
#include "mechanics/Player.h"
#include "mechanics/ScenarioController.h"
#include "mechanics/UnitManager.h"

#include "render/Camera.h"
#ifdef USE_SDL2
#include "render/SdlRenderTarget.h"
#include <SDL2/SDL.h>
#else
#include "render/SfmlRenderTarget.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowStyle.hpp>
#endif
#include "render/MapRenderer.h"
#include "render/UnitsRenderer.h"

#include "ui/ActionPanel.h"
#include "ui/Dialog.h"
#include "ui/IconButton.h"
#include "ui/Minimap.h"
#include "ui/NumberLabel.h"
#include "ui/UiScreen.h"
#include "ui/UnitInfoPanel.h"

#include "resource/AssetManager.h"
#include "resource/Resource.h"
#include "ui/MouseCursor.h"
#include "settings/input.h"

#include <genie/dat/ResourceUsage.h>
#include <genie/resource/SlpFile.h>
#include <genie/resource/UIFile.h>

#include <thread>

#include <algorithm>
#include <utility>

#include <cstddef>

#define MOUSE_MOVE_EDGE_SIZE 10
#define CAMERA_SPEED 1.

#ifndef USE_SDL2
static input::Key sfKeyToInput(sf::Keyboard::Key key) {
    switch(key) {
    case sf::Keyboard::Left: return input::Key::Left;
    case sf::Keyboard::Right: return input::Key::Right;
    case sf::Keyboard::Up: return input::Key::Up;
    case sf::Keyboard::Down: return input::Key::Down;
    case sf::Keyboard::Escape: return input::Key::Escape;
    case sf::Keyboard::Return: return input::Key::Return;
    case sf::Keyboard::Delete: return input::Key::Delete;
    case sf::Keyboard::BackSpace: return input::Key::BackSpace;
    case sf::Keyboard::Space: return input::Key::Space;
    case sf::Keyboard::Tab: return input::Key::Tab;
    case sf::Keyboard::F5: return input::Key::F5;
    default:
        if (key >= sf::Keyboard::A && key <= sf::Keyboard::Z)
            return input::Key(int(input::Key::A) + (key - sf::Keyboard::A));
        if (key >= sf::Keyboard::Num0 && key <= sf::Keyboard::Num9)
            return input::Key(int(input::Key::Num0) + (key - sf::Keyboard::Num0));
        if (key >= sf::Keyboard::F1 && key <= sf::Keyboard::F12)
            return input::Key(int(input::Key::F1) + (key - sf::Keyboard::F1));
        return input::Key::Unknown;
    }
}

static input::MouseButton sfMouseButtonToInput(sf::Mouse::Button button) {
    switch(button) {
    case sf::Mouse::Left: return input::MouseButton::Left;
    case sf::Mouse::Right: return input::MouseButton::Right;
    case sf::Mouse::Middle: return input::MouseButton::Middle;
    default: return input::MouseButton::Left;
    }
}

static input::Event sfEventToInput(const sf::Event &sfEvent) {
    input::Event ev{};
    switch(sfEvent.type) {
    case sf::Event::Closed:
        ev.type = input::Event::Closed;
        break;
    case sf::Event::KeyPressed:
        ev.type = input::Event::KeyPressed;
        ev.key.code = sfKeyToInput(sfEvent.key.code);
        ev.key.shift = sfEvent.key.shift;
        ev.key.control = sfEvent.key.control;
        ev.key.alt = sfEvent.key.alt;
        break;
    case sf::Event::KeyReleased:
        ev.type = input::Event::KeyReleased;
        ev.key.code = sfKeyToInput(sfEvent.key.code);
        ev.key.shift = sfEvent.key.shift;
        ev.key.control = sfEvent.key.control;
        ev.key.alt = sfEvent.key.alt;
        break;
    case sf::Event::MouseButtonPressed:
        ev.type = input::Event::MouseButtonPressed;
        ev.mouseButton.button = sfMouseButtonToInput(sfEvent.mouseButton.button);
        ev.mouseButton.x = sfEvent.mouseButton.x;
        ev.mouseButton.y = sfEvent.mouseButton.y;
        break;
    case sf::Event::MouseButtonReleased:
        ev.type = input::Event::MouseButtonReleased;
        ev.mouseButton.button = sfMouseButtonToInput(sfEvent.mouseButton.button);
        ev.mouseButton.x = sfEvent.mouseButton.x;
        ev.mouseButton.y = sfEvent.mouseButton.y;
        break;
    case sf::Event::MouseMoved:
        ev.type = input::Event::MouseMoved;
        ev.mouseMove.x = sfEvent.mouseMove.x;
        ev.mouseMove.y = sfEvent.mouseMove.y;
        break;
    case sf::Event::MouseWheelScrolled:
        ev.type = input::Event::MouseWheelScrolled;
        ev.mouseWheel.delta = sfEvent.mouseWheelScroll.delta;
        ev.mouseWheel.x = sfEvent.mouseWheelScroll.x;
        ev.mouseWheel.y = sfEvent.mouseWheelScroll.y;
        break;
    case sf::Event::TextEntered:
        ev.type = input::Event::TextEntered;
        ev.text.unicode = sfEvent.text.unicode;
        break;
    default:
        ev.type = input::Event::Closed; // fallback
        break;
    }
    return ev;
}
#endif // !USE_SDL2

//------------------------------------------------------------------------------
void Engine::start()
{
    DBG << "Starting engine.";
    std::shared_ptr<GameState> state;

    ScreenPos mousePos;
    // Start the game loop
    size_t fpsSamples = 0;
    double totalFps = 0;
#ifdef USE_SDL2
    while (m_sdlWindow->isOpen()) {
#else
    while (renderWindow_->isOpen()) {
#endif
        if (state != state_manager_.getActiveState()) {
            state = state_manager_.getActiveState();
            m_minimap->setUnitManager(state->unitManager());
            m_minimap->setMap(state->map());
            m_minimap->setVisibilityMap(state->humanPlayer()->visibility);

            m_mapRenderer->setVisibilityMap(state->humanPlayer()->visibility);
            m_mapRenderer->setMap(state->map());

            m_actionPanel->setUnitManager(state->unitManager());
            m_actionPanel->setHumanPlayer(state->humanPlayer());
            m_unitInfoPanel->setUnitManager(state->unitManager());

            m_unitsRenderer->setUnitManager(state->unitManager());
            m_unitsRenderer->setVisibilityMap(state->humanPlayer()->visibility);
        }

        const int renderStart = Engine::currentTimeMs();

        bool updated = false;

        // Process events
#ifdef USE_SDL2
        input::Event event;
        while (m_sdlWindow->pollEvent(event)) {
            if (event.type == input::Event::Closed) {
                m_sdlWindow->close();
            }

            if (event.type == input::Event::MouseButtonPressed || event.type == input::Event::MouseButtonReleased) {
                mousePos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
            }

            if (event.type == input::Event::MouseMoved) {
                mousePos = ScreenPos(event.mouseMove.x, event.mouseMove.y);
            }

            if (!handleEvent(event, state)) {
            }

            updated = true;
        }

#else
        sf::Event sfEvent;
        while (renderWindow_->pollEvent(sfEvent)) {
            // Close window : exit
            if (sfEvent.type == sf::Event::Closed) {
                renderWindow_->close();
            }

            input::Event event = sfEventToInput(sfEvent);

            if (event.type == input::Event::MouseButtonPressed || event.type == input::Event::MouseButtonReleased) {
                sf::Vector2f mappedPos = renderWindow_->mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
                event.mouseButton.x = mappedPos.x;
                event.mouseButton.y = mappedPos.y;
                mousePos = ScreenPos(mappedPos);
            }

            if (event.type == input::Event::MouseMoved) {
                sf::Vector2f mappedPos = renderWindow_->mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y));
                event.mouseMove.x = mappedPos.x;
                event.mouseMove.y = mappedPos.y;
                mousePos = ScreenPos(mappedPos);
            }

            if (!handleEvent(event, state)) {
//                state->handleEvent(event);
            }

            updated = true;
        }
#endif

        if (!m_currentDialog && state->result == GameState::Result::Running) {
            updated = state->update(Engine::currentTimeMs()) || updated;

            if (state->result != GameState::Result::Running) {
                if (state->result == GameState::Result::Won) {
                    m_resultOverlay->string = "You are victorious!";
                } else {
                    m_resultOverlay->string = "You have been defeated!"; // TODO: don't remember the exact text
                }
                const Size windowSize = renderTarget_->getSize();
                m_resultOverlay->position = ScreenPos(windowSize.width / 2, windowSize.height / 2);
            }
        }

        if (m_mouseCursor) updated = m_mouseCursor->setPosition(mousePos) || updated;
        updated = updateUi(state) || updated;


        if (m_selecting) {
            ScreenRect selectionRect(m_selectionStart, m_selectionCurr);
            if (selectionRect != m_selectionRect) {
                m_selectionRect = selectionRect;
                updated = true;
            }
        }


        if (updated) {
            // Clear screen
#ifdef USE_SDL2
            auto *sdlRT = static_cast<SdlRenderTarget*>(renderTarget_.get());
            SDL_Renderer *ren = sdlRT->renderer();

            // Render game world to off-screen texture
            SDL_SetRenderTarget(ren, m_gameTexture);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
#else
            renderWindow_->clear(sf::Color::Green);
#endif
            m_mapRenderer->display();

            drawEntities(state->map());

            state->draw();

            if (m_currentDialog) {
#ifdef USE_SDL2
                m_currentDialog->render(renderTarget_);
#else
                m_currentDialog->render(renderWindow_);
#endif
            }

            if (state->result != GameState::Result::Running) {
                renderTarget_->draw(m_resultOverlay);
            }

#ifdef USE_SDL2
            // Switch to screen, blit game texture with zoom
            SDL_SetRenderTarget(ren, NULL);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);

            int texW = static_cast<int>(m_baseViewportSize.width);
            int texH = static_cast<int>(m_baseViewportSize.height);
            // Zoom: smaller srcrect = magnified view
            int srcW = static_cast<int>(texW / m_zoomLevel);
            int srcH = static_cast<int>(texH / m_zoomLevel);
            int srcX = (texW - srcW) / 2;
            int srcY = (texH - srcH) / 2;
            SDL_Rect src = {srcX, srcY, srcW, srcH};
            SDL_Rect dst = {0, 0, texW, texH};
            SDL_RenderCopy(ren, m_gameTexture, &src, &dst);
#endif
            // HUD at 1:1 on top
            drawUi();

            const int renderTime = Engine::currentTimeMs() - renderStart;

            if (renderTime > 0) {
                fpsSamples++;
                totalFps += 1000. / renderTime;
                fps_label_->string = "fps: " + std::to_string(1000/renderTime);
            }

            // Update the window
#ifdef USE_SDL2
            m_sdlWindow->display();
#else
            renderWindow_->display();
#endif
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
        }

    }

    if (totalFps > 0 && fpsSamples > 0) {
        DBG << "avg fps:" << (totalFps / fpsSamples);
    }
}

void Engine::addMessage(const std::string &message)
{
    for (int i=0; i<s_numMessagesLines - 1; i++) {
        m_visibleText[i].text->string = m_visibleText[i+1].text->string;
        m_visibleText[i].endTime = m_visibleText[i+1].endTime;
    }
    m_visibleText[s_numMessagesLines - 1].text->string = message;
    m_visibleText[s_numMessagesLines - 1].endTime = Engine::currentTimeMs() + s_messageTimeout;
}

void Engine::showStartScreen()
{
    genie::UIFilePtr uiFile = AssetManager::Inst()->getUIFile("scrstart.sin");
    if (!uiFile) {
        WARN << "failed to load ui file for start screen";
        return;
    }

    genie::SlpFilePtr loadingImageFile = AssetManager::Inst()->getSlp(uiFile->backgroundSmall.fileId);
    if (!loadingImageFile) {
        WARN << "Failed to load background for start screen" << uiFile->backgroundSmall.filename << uiFile->backgroundSmall.alternateFilename;
        return;
    }

    Drawable::Image::Ptr loadingScreen = renderTarget_->convertFrameToImage(
                                    loadingImageFile->getFrame(0),
                                    AssetManager::Inst()->getPalette(uiFile->paletteFile.id)
                                    );

    loadingScreen->scaleX = renderTarget_->getSize().width / float(loadingScreen->size.width);
    loadingScreen->scaleY = renderTarget_->getSize().height / float(loadingScreen->size.height);

    renderTarget_->draw(loadingScreen, ScreenPos(0, 0));
#ifdef USE_SDL2
    m_sdlWindow->display();
#else
    renderWindow_->display();
#endif
}

void Engine::loadTopButtons()
{
    float x = renderTarget_->getSize().width - 5;
    for (int i=0; i<IconButton::ButtonsCount; i++) {
        std::unique_ptr<IconButton> button = std::make_unique<IconButton>(renderTarget_);

        button->setType(IconButton::Type(i));

        x -= button->rect().width;
        button->setPosition({x, 5});

        m_buttons.push_back(std::move(button));
    }
}

void Engine::loadUiOverlay()
{
    std::shared_ptr<genie::SlpFile> overlayFile = AssetManager::Inst()->getUiOverlay(AssetManager::Ui1280x1024, AssetManager::Viking);
    if (overlayFile) {
        m_uiOverlay = renderTarget_->convertFrameToImage(overlayFile->getFrame());
        DBG << "Loaded UI overlay with size" << m_uiOverlay->size;
    } else {
        AssetManager::UiResolution attemptedResolution = AssetManager::Ui1280x1024;
        AssetManager::UiCiv attemptedCiv = AssetManager::Briton;
        do {
            attemptedCiv = AssetManager::UiCiv(attemptedCiv + 1);
            if (attemptedCiv > AssetManager::Korean) {
                if (attemptedResolution == AssetManager::Ui1280x1024) {
                    attemptedResolution = AssetManager::Ui1024x768;
                } else if (attemptedResolution == AssetManager::Ui1024x768) {
                    attemptedResolution = AssetManager::Ui800x600;
                } else {
                    break;
                }

                attemptedCiv = AssetManager::Briton;
            }
            overlayFile = AssetManager::Inst()->getUiOverlay(attemptedResolution, attemptedCiv);
        } while (!overlayFile);

        if (overlayFile) {
            WARN << "Loaded fallback ui overlay res" << attemptedResolution << "for civ" << attemptedCiv;
            m_uiOverlay = renderTarget_->convertFrameToImage(overlayFile->getFrame());
        } else {
            WARN << "Failed to load ui overlay";
        }
    }
}

void Engine::drawUi()
{
    if (m_selecting) {
        renderTarget_->draw(m_selectionRect, Drawable::Transparent, Drawable::White);
    }

#ifdef ANDROID
    if (m_uiOverlay && m_uiOverlay->isValid()) {
        float scaleX = renderTarget_->getSize().width / m_uiOverlay->size.width;
        m_uiOverlay->scaleX = scaleX;
        m_uiOverlay->scaleY = scaleX;
        float overlayH = m_uiOverlay->size.height * scaleX;
        renderTarget_->draw(m_uiOverlay, ScreenPos(0, renderTarget_->getSize().height - overlayH));
    }
#else
    renderTarget_->draw(m_uiOverlay, ScreenPos(0, m_uiOverlayOffset));
#endif

    for (const std::unique_ptr<IconButton> &button : m_buttons) {
        button->render();
    }

    m_minimap->draw();
    m_actionPanel->draw();
    m_unitInfoPanel->draw();

    m_woodLabel->render();
    m_foodLabel->render();
    m_goldLabel->render();
    m_stoneLabel->render();
    m_populationLabel->render();

    renderTarget_->draw(fps_label_);

    const Time currentTime = Engine::currentTimeMs();
    for (const MessageLine &messageLine : m_visibleText) {
        if (messageLine.endTime < currentTime) {
            continue;
        }
        renderTarget_->draw(messageLine.text);
    }

#ifndef ANDROID
    if (m_mouseCursor) m_mouseCursor->render();
#endif
}

void Engine::drawEntities(const std::shared_ptr<Map> &map)
{
    TIME_THIS;

    const int firstCol = m_mapRenderer->firstVisibleColumn();
    const int lastCol = m_mapRenderer->lastVisibleColumn();
    const int firstRow = m_mapRenderer->firstVisibleRow();
    const int lastRow = m_mapRenderer->lastVisibleRow();

    m_unitsRenderer->begin(renderTarget_);
    std::vector<EntityPtr> visibleEntities;
    for (int col = firstCol; col <  lastCol; col++) {
        for (int row = firstRow; row <  lastRow; row++) {
            for (const std::weak_ptr<Entity> &e : map->entitiesAt(col, row)) {
                visibleEntities.push_back(e.lock());;

            }
        }
    }
    m_unitsRenderer->render(renderTarget_, visibleEntities);
    m_unitsRenderer->display(renderTarget_);
}

bool Engine::handleEvent(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    if (m_currentDialog) {
        Dialog::Choice choice = m_currentDialog->handleEvent(event);
        if (choice == Dialog::Cancel) {
            m_currentDialog.reset();
        } else if (choice == Dialog::Quit) {
#ifdef USE_SDL2
            m_sdlWindow->close();
#else
            renderWindow_->close();
#endif
        }

        return true;
    }

    if (m_actionPanel->handleEvent(event)) {
        return true;
    }
    if (m_minimap->handleEvent(event)) {
        return true;
    }
    if (m_unitInfoPanel->handleEvent(event)) {
        return true;
    }

    switch(event.type) {
    case input::Event::KeyPressed:
        return handleKeyEvent(event, state);
    case input::Event::MouseButtonPressed:
        return handleMousePress(event, state);
    case input::Event::MouseButtonReleased:
        return handleMouseRelease(event, state);
    case input::Event::MouseMoved:
        return handleMouseMove(event, state);
    case input::Event::TouchBegan:
    case input::Event::TouchMoved:
    case input::Event::TouchEnded:
        return handleTouchEvent(event, state);
    case input::Event::PinchZoom: {
        m_zoomLevel = std::clamp(m_zoomLevel + event.pinch.dDist * PINCH_SENSITIVITY, ZOOM_MIN, ZOOM_MAX);
        // Adjust camera viewport so it knows which map area is visible at this zoom
        Size viewportSize(m_baseViewportSize.width / m_zoomLevel, m_baseViewportSize.height / m_zoomLevel);
        renderTarget_->camera()->setViewportSize(viewportSize);
        return true;
    }
    default:
        break;
    }

    return false;
}

bool Engine::handleKeyEvent(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    ScreenPos cameraScreenPos = renderTarget_->camera()->targetPosition().toScreen();

    switch(event.key.code) {
    case input::Key::Left:
        cameraScreenPos.x -= 20;
        break;

    case input::Key::Right:
        cameraScreenPos.x += 20;
        break;

    case input::Key::Down:
        cameraScreenPos.y -= 20;
        break;

    case input::Key::Up:
        cameraScreenPos.y += 20;
        break;

    default:
        return false;
    }

    MapPos cameraMapPos = cameraScreenPos.toMap().clamped(state->map()->pixelSize());

    renderTarget_->camera()->setTargetPosition(cameraMapPos);

    return true;

}

bool Engine::handleMouseMove(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    const ScreenPos mousePos = ScreenPos(event.mouseMove.x, event.mouseMove.y);
    bool handled = false;


    if (mousePos.x < MOUSE_MOVE_EDGE_SIZE) {
        m_cameraDeltaX = -1;
        handled = true;
    } else if (mousePos.x > renderTarget_->getSize().width - MOUSE_MOVE_EDGE_SIZE) {
        m_cameraDeltaX = 1;
        handled = true;
    } else {
        m_cameraDeltaX = 0;
    }

    if (mousePos.y < MOUSE_MOVE_EDGE_SIZE) {
        m_cameraDeltaY = 1;
        handled = true;
    } else if (mousePos.y > renderTarget_->getSize().height - MOUSE_MOVE_EDGE_SIZE) {
        m_cameraDeltaY = -1;
        handled = true;
    } else {
        m_cameraDeltaY = 0;
    }

    if (mousePos.y < m_gameAreaHeight) {
        if (m_selecting) {
            m_selectionCurr = mousePos;
            handled = true;
        } else {
            state->unitManager()->onMouseMove(renderTarget_->camera()->absoluteMapPos(mousePos));
        }
    }

    return handled;
}

bool Engine::handleMousePress(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    const ScreenPos mousePos(event.mouseButton.x, event.mouseButton.y);
    bool updated = false;
    for (const std::unique_ptr<IconButton> &button : m_buttons) {
        updated = button->onMousePressed(mousePos) || updated;
    }
    if (updated) {
        return true;
    }

    if (mousePos.y < m_gameAreaHeight && event.mouseButton.button == input::MouseButton::Left) {
        if (state->unitManager()->onLeftClick(ScreenPos(event.mouseButton.x, event.mouseButton.y), renderTarget_->camera())) {
            return true;
        }

        m_selectionStart = mousePos;
        m_selectionCurr = mousePos + ScreenPos(1, 1);
        m_selecting = true;
    }

    return true;
}

bool Engine::handleTouchEvent(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    float tx = static_cast<float>(event.touch.x);
    float ty = static_cast<float>(event.touch.y);

    switch (event.type) {
    case input::Event::TouchBegan:
        m_touchState.active = true;
        m_touchState.startPos = ScreenPos(tx, ty);
        m_touchState.lastPos = m_touchState.startPos;
        m_touchState.startTime = currentTimeMs();
        m_touchState.dragging = false;
        return false; // Let mouse emulation handle tap
    case input::Event::TouchMoved: {
        ScreenPos pos(tx, ty);
        if (!m_touchState.dragging) {
            if (m_touchState.startPos.distanceTo(pos) > TouchState::DRAG_THRESHOLD) {
                m_touchState.dragging = true;
            }
        }
        if (m_touchState.dragging) {
            ScreenPos delta = m_touchState.lastPos - pos;
            ScreenPos camScreen = renderTarget_->camera()->targetPosition().toScreen();
            camScreen.x += delta.x;
            camScreen.y -= delta.y;
            MapPos camMap = camScreen.toMap().clamped(state->map()->pixelSize());
            renderTarget_->camera()->setTargetPosition(camMap);
        }
        m_touchState.lastPos = pos;
        return false; // Let mouse emulation also fire
    }
    case input::Event::TouchEnded: {
        if (!m_touchState.dragging) {
            ScreenPos pos(tx, ty);
            int64_t now = currentTimeMs();
            if (m_touchState.hasPendingTap
                && (now - m_touchState.pendingTapTime < TouchState::DOUBLE_TAP_MS)
                && m_touchState.pendingTapPos.distanceTo(pos) < TouchState::DOUBLE_TAP_DIST) {
                // Double tap = right click
                state->unitManager()->onRightClick(pos, renderTarget_->camera());
                m_touchState.hasPendingTap = false;
            } else {
                m_touchState.hasPendingTap = true;
                m_touchState.pendingTapTime = now;
                m_touchState.pendingTapPos = pos;
            }
        }
        m_touchState.active = false;
        m_touchState.dragging = false;
        return false; // Let mouse emulation handle click
    }
    default:
        return false;
    }
}

bool Engine::handleMouseRelease(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    const ScreenPos mousePos(event.mouseButton.x, event.mouseButton.y);

    if (mousePos.y < m_gameAreaHeight && event.mouseButton.button == input::MouseButton::Left) {
        if (state->unitManager()->onMouseRelease()) {
            return true;
        }
    }

    IconButton::Type clickedButton = IconButton::Invalid;
    for (const std::unique_ptr<IconButton> &button : m_buttons) {
        if (button->onMouseReleased(mousePos)) {
            clickedButton = button->type();
        }
    }
    if (clickedButton == IconButton::GameMenu) {
        showMenu();
    }
    if (clickedButton != IconButton::Invalid) {
        return true;
    }

    if (event.mouseButton.button == input::MouseButton::Left && m_selecting) {
        state->unitManager()->selectUnits(m_selectionRect, renderTarget_->camera());
        m_selectionRect = ScreenRect();
        m_selecting = false;
        return true;
    }
    if (event.mouseButton.button == input::MouseButton::Right) {
        state->unitManager()->onRightClick(mousePos, renderTarget_->camera());
    }

    return false;
}

//------------------------------------------------------------------------------
Engine::Engine()
{
    m_mainScreen = std::make_unique<UiScreen>("dlg_men.sin");
}

// Just to make the crappy gcc unique_ptr implementation work, we can't use = default
Engine::~Engine()
{
#ifdef USE_SDL2
    if (m_gameTexture) { SDL_DestroyTexture(m_gameTexture); m_gameTexture = nullptr; }
#endif
}

bool Engine::setup(const std::shared_ptr<genie::ScnFile> &scenario)
{
#ifdef USE_SDL2
#ifdef ANDROID
    // Force landscape orientation and create fullscreen window
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    // Keep touch→mouse emulation ON (default) — taps auto-generate mouse clicks
    // FINGER events used only for camera drag and pinch zoom
    m_sdlWindow = std::make_unique<SdlWindow>(Size(0, 0), "freeaoe");
    SDL_SetWindowFullscreen(m_sdlWindow->sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
    // Get actual window size after fullscreen
    int screenW, screenH;
    SDL_GetWindowSize(m_sdlWindow->sdlWindow, &screenW, &screenH);
    // Ensure landscape (swap if portrait)
    if (screenH > screenW) { std::swap(screenW, screenH); }
    SDL_SetWindowSize(m_sdlWindow->sdlWindow, screenW, screenH);
#else
    m_sdlWindow = std::make_unique<SdlWindow>(Size(1280, 1024), "freeaoe");
#endif
    // Non-owning shared_ptr — SdlWindow owns the render target lifetime
    renderTarget_ = std::shared_ptr<IRenderTarget>(m_sdlWindow->renderTarget.get(), [](IRenderTarget*){});
    m_mainScreen->init();
#else
    renderWindow_ = std::make_unique<sf::RenderWindow>(sf::VideoMode(1280, 1024), "freeaoe", sf::Style::None);
    renderWindow_->setFramerateLimit(60);

    m_mainScreen->setRenderWindow(renderWindow_);
    m_mainScreen->init();

    renderTarget_ = std::make_shared<SfmlRenderTarget>(*renderWindow_);
#endif

    m_mouseCursor = std::make_unique<MouseCursor>(renderTarget_);
#ifdef ANDROID
    SDL_ShowCursor(SDL_DISABLE);
#elif defined(USE_SDL2)
    if (m_mouseCursor->isValid()) {
        SDL_ShowCursor(SDL_DISABLE);
    }
#else
    if (m_mouseCursor->isValid()) {
        renderWindow_->setMouseCursorVisible(false);
    }
#endif

    m_woodLabel = std::make_unique<NumberLabel>(renderTarget_);
    m_foodLabel = std::make_unique<NumberLabel>(renderTarget_);
    m_goldLabel = std::make_unique<NumberLabel>(renderTarget_);
    m_stoneLabel = std::make_unique<NumberLabel>(renderTarget_);
    m_populationLabel = std::make_unique<NumberLabel>(renderTarget_);

    m_unitsRenderer = std::make_unique<UnitsRenderer>();

    m_woodLabel->setPosition({75, 5});
    m_foodLabel->setPosition({153, 5});
    m_goldLabel->setPosition({230, 5});
    m_stoneLabel->setPosition({307, 5});
    m_populationLabel->setPosition({384, 5});

    showStartScreen();

    std::shared_ptr<GameState> gameState = std::make_shared<GameState>(renderTarget_);
    if (scenario) {
        gameState->setScenario(scenario);
    }
    gameState->scenarioController()->setEngine(this);

    if (!state_manager_.addActiveState(gameState)) {
        return false;
    }

    m_minimap = std::make_unique<Minimap>(renderTarget_);
    REQUIRE(m_minimap->init(), return false);

    m_actionPanel = std::make_unique<ActionPanel>(renderTarget_);
    REQUIRE(m_actionPanel->init(), return false);

    m_unitInfoPanel = std::make_unique<UnitInfoPanel>(renderTarget_);
    REQUIRE(m_unitInfoPanel->init(), return false);

    m_mapRenderer = std::make_unique<MapRenderer>();
    m_mapRenderer->setRenderTarget(renderTarget_);

    loadUiOverlay();

    Size uiSize = m_uiOverlay->size;
    if (m_uiOverlay->size.isValid()) {
        switch(int(uiSize.width)) {
        case 1600:
            uiSize.height = 1200;
            break;
        case 1280:
            uiSize.height = 1024;
            break;
        case 1024:
            uiSize.height = 768;
            break;
        case 800:
            uiSize.height = 600;
            break;
        default:
            WARN << "unknown ui width:" << uiSize.width;
            break;
        }
        m_uiOverlayOffset = uiSize.height - m_uiOverlay->size.height;
    } else {
        WARN << "We don't have a valid UI overlay";
        uiSize = Size(640, 480);
    }

#ifdef ANDROID
    // Set logical render size matching phone aspect ratio — fills entire screen
    {
        int sw, sh;
        SDL_GetWindowSize(m_sdlWindow->sdlWindow, &sw, &sh);
        // Keep width 1280, calculate height to match screen aspect ratio
        int logicalH = 1280 * sh / sw;
        SDL_RenderSetLogicalSize(
            static_cast<SdlRenderTarget*>(renderTarget_.get())->renderer(),
            1280, logicalH
        );
        uiSize = Size(1280, logicalH);
    }
#elif defined(USE_SDL2)
    SDL_SetWindowSize(m_sdlWindow->sdlWindow, uiSize.width, uiSize.height);
#else
    renderWindow_->setSize(uiSize);
#endif
    renderTarget_->setSize(uiSize);
    m_baseViewportSize = uiSize;

#ifdef USE_SDL2
    {
        auto *sdlRT = static_cast<SdlRenderTarget*>(renderTarget_.get());
        m_gameTexture = SDL_CreateTexture(sdlRT->renderer(),
            SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
            static_cast<int>(uiSize.width), static_cast<int>(uiSize.height));
    }
#endif

    // Calculate game area height (screen minus UI overlay)
    if (m_uiOverlay && m_uiOverlay->size.isValid()) {
        m_gameAreaHeight = uiSize.height - m_uiOverlay->size.height + m_uiOverlayOffset;
    } else {
        m_gameAreaHeight = uiSize.height * 0.75f;
    }

    m_resultOverlay = renderTarget_->createText(Drawable::Text::UI);
    m_resultOverlay->alignment = Drawable::Text::AlignCenter;
    m_resultOverlay->color = Drawable::White;
    m_resultOverlay->pointSize = 25;
    m_resultOverlay->outlineColor = Drawable::Black;

    fps_label_ = renderTarget_->createText(Drawable::Text::UI);
    fps_label_->position = ScreenPos(uiSize.width - 75, uiSize.height - 20);
    fps_label_->color = Drawable::White;
    fps_label_->pointSize = 15;

    m_woodLabel->setValue(12345);
    m_foodLabel->setValue(12345);
    m_goldLabel->setValue(12345);
    m_stoneLabel->setValue(12345);
    m_populationLabel->setValue(125);
    m_populationLabel->setMaxValue(125);

    loadTopButtons();

    int posY = 30;
    for (int i=0; i<s_numMessagesLines; i++) {
        Drawable::Text::Ptr text = renderTarget_->createText();
        text->pointSize = 14;
        text->position.x = 5;
        text->position.y = posY;
        text->outlineColor = Drawable::Black;
        text->color = Drawable::White;

        posY += text->lineSpacing();

        m_visibleText[i].text = text;
    }

    return true;
}

void Engine::showMenu()
{
    genie::UIFilePtr uiFile = AssetManager::Inst()->getUIFile("dlg_men.sin");
    if (!uiFile) {
        WARN << "failed to load ui file for menu";
        return;
    }

    genie::SlpFilePtr backgroundSlp = AssetManager::Inst()->getSlp(uiFile->backgroundSmall.fileId);
    if (!backgroundSlp) {
        WARN << "Failed to load menu background";
        return;
    }
    Resource::RawImage menuBg = Resource::convertFrameToImage(backgroundSlp->getFrame(0));

    m_currentDialog = std::make_unique<Dialog>(m_mainScreen.get());
    m_currentDialog->background = renderTarget_->createImage(Size(menuBg.width, menuBg.height), menuBg.pixels.data());
    DBG << "showing menu";

}

bool Engine::updateUi(const std::shared_ptr<GameState> &state)
{
    const int deltaTime = Engine::currentTimeMs() - m_lastUpdate;

    bool updated = false;

    const Player::Ptr &humanPlayer = state->humanPlayer();
    updated = m_woodLabel->setValue(humanPlayer->resourcesAvailable(genie::ResourceType::WoodStorage)) || updated;
    updated = m_foodLabel->setValue(humanPlayer->resourcesAvailable(genie::ResourceType::FoodStorage)) || updated;
    updated = m_goldLabel->setValue(humanPlayer->resourcesAvailable(genie::ResourceType::GoldStorage)) || updated;
    updated = m_stoneLabel->setValue(humanPlayer->resourcesAvailable(genie::ResourceType::StoneStorage)) || updated;

    updated = m_populationLabel->setValue(humanPlayer->resourcesUsed(genie::ResourceType::PopulationHeadroom)) || updated;
    updated = m_populationLabel->setMaxValue(humanPlayer->resourcesAvailable(genie::ResourceType::PopulationHeadroom)) || updated;

    if (m_mouseCursor) updated = m_mouseCursor->update(state->unitManager()) || updated;

    updated = m_mapRenderer->update(Engine::currentTimeMs()) || updated;

    updated = updateCamera(state) || updated;

    updated = m_minimap->update(deltaTime) || updated;
    updated = m_actionPanel->update(deltaTime) || updated;
    updated = m_unitInfoPanel->update(deltaTime) || updated;

    m_lastUpdate = Engine::currentTimeMs();
    return updated;
}

bool Engine::updateCamera(const std::shared_ptr<GameState> &state)
{
#ifdef ANDROID
    return false; // Camera controlled by touch drag
#endif
    if (m_cameraDeltaX == 0 && m_cameraDeltaY == 0) {
        return false;
    }

    ScreenPos cameraScreenPos = renderTarget_->camera()->targetPosition().toScreen();

    const int deltaTime = Engine::currentTimeMs() - m_lastUpdate;
    cameraScreenPos.x += m_cameraDeltaX * deltaTime * CAMERA_SPEED;
    cameraScreenPos.y += m_cameraDeltaY * deltaTime * CAMERA_SPEED;

    MapPos cameraMapPos = cameraScreenPos.toMap().clamped(state->map()->pixelSize());
    renderTarget_->camera()->setTargetPosition(cameraMapPos);


    if (m_selecting) {
        m_selectionStart.x -= m_cameraDeltaX * deltaTime * CAMERA_SPEED;
        m_selectionStart.y += m_cameraDeltaY * deltaTime * CAMERA_SPEED;
        m_selectionRect = ScreenRect(m_selectionStart, m_selectionCurr);
    }

    return true;
}
