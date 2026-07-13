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
#include "audio/AudioPlayer.h"
#include "ui/DiplomacyScreen.h"
#include "ui/SettingsScreen.h"
#include "ui/TechTreeScreen.h"
#include "mechanics/SaveGame.h"

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
void Engine::setupRandomMap(int mapType, int mapSize, int playerCount)
{
    auto state = state_manager_.getActiveState();
    if (!state) return;

    // Two-phase random map setup:
    // Phase 1: create players and terrain (no units yet)
    // Phase 2: wire up renderers, then place units (which trigger visibility events)

    state->setupRandomMap(mapType, mapSize, playerCount);

    // Wire up renderers after map and players exist
    if (state->humanPlayer()) {
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
}

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

        // Deferred deselect: if first tap expired without second tap, deselect
        if (m_touchState.phase == TouchState::Phase::WaitSecondTap) {
            int64_t elapsed = currentTimeMs() - m_touchState.tapTime;
            if (elapsed >= TouchState::DOUBLE_TAP_MS) {
                m_touchState.phase = TouchState::Phase::Idle;
                // Tap on empty ground with no follow-up → clear selection
                if (!state->unitManager()->selected().isEmpty()) {
                    // Check if there's actually a unit at the tap position
                    bool unitAtTap = state->unitManager()->unitAt(
                        m_touchState.tapPos, renderTarget_->camera(), NoAlignment) != nullptr;
                    if (!unitAtTap) {
                        // Empty ground — just deselect everything
                        ScreenRect emptyRect(ScreenPos(-100, -100), ScreenPos(-99, -99));
                        state->unitManager()->selectUnits(emptyRect, renderTarget_->camera());
                    }
                    updated = true;
                }
            }
        }

        // Long-press detection: if finger held down > 600ms = right-click (move/attack)
        if (m_touchState.phase == TouchState::Phase::Pending) {
            int64_t held = currentTimeMs() - m_touchState.startTime;
            if (held >= TouchState::LONG_PRESS_MS && !state->unitManager()->selected().isEmpty()) {
                // Long-press with units selected = right click (move/attack)
                state->unitManager()->onRightClick(m_touchState.startPos, renderTarget_->camera());
                m_touchState.tapTime = 0; // Cancel deferred deselect
                m_touchState.phase = TouchState::Phase::LongPress; // Keep LongPress so release handler doesn't re-select
                updated = true;
            }
        }

        AudioPlayer::instance().tick(); // Drain queued dialogue streams

        // Start background music if not playing
        {
            static bool musicStarted = false;
            if (!musicStarted) {
                // Try random music track
                int track = 1 + (rand() % 9);
                std::string musicFile = "xmusic" + std::to_string(track) + ".mp3";
                AudioPlayer::instance().playStream(musicFile);
                musicStarted = true;
            }
        }

        if (!m_currentDialog && !m_paused && state->result == GameState::Result::Running) {
            // Scale game time by speed (currentTimeMs is real time, we need game time)
            static Time lastRealTime = Engine::currentTimeMs();
            static Time gameTime = 0;
            Time nowReal = Engine::currentTimeMs();
            gameTime += static_cast<Time>((nowReal - lastRealTime) * m_gameSpeed);
            lastRealTime = nowReal;
            updated = state->update(gameTime) || updated;

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
            bool useZoomTexture = (m_zoomLevel != 1.0f) && m_gameTexture;

            if (useZoomTexture) {
                SDL_SetRenderTarget(ren, m_gameTexture);
            }
            renderTarget_->clear(Drawable::Black);
#else
            renderWindow_->clear(sf::Color::Green);
#endif
            m_mapRenderer->display();

            drawEntities(state->map());

            state->draw();

            // Dialog and result overlay rendered AFTER drawUi() — see below

            if (state->result != GameState::Result::Running) {
                // Semi-transparent overlay
                Size ws = renderTarget_->getSize();
                renderTarget_->draw(ScreenRect(0, 0, ws.width, ws.height),
                                    Drawable::Color(0, 0, 0, 180));
                renderTarget_->draw(m_resultOverlay);

                // Post-game statistics
                const Player::Ptr &human = state->humanPlayer();
                if (human) {
                    float sy = ws.height / 2.f + 40;
                    auto statText = renderTarget_->createText(Drawable::Text::Plain);
                    statText->pointSize = 14;
                    statText->color = Drawable::Color(200, 190, 150, 255);

                    auto drawStat = [&](const std::string &label, int value) {
                        statText->string = label + ": " + std::to_string(value);
                        statText->position = ScreenPos(ws.width / 2 - 100, sy);
                        renderTarget_->draw(statText);
                        sy += 22;
                    };

                    drawStat("Score", human->score());
                    drawStat("Units Killed", human->unitsKilled);
                    drawStat("Units Lost", human->unitsLost);
                    drawStat("Buildings Razed", human->buildingsRazed);
                    drawStat("Techs Researched", human->techsResearched);

                    statText->string = "Tap Menu to exit";
                    statText->color = Drawable::Color(150, 140, 110, 200);
                    statText->position = ScreenPos(ws.width / 2 - 70, sy + 15);
                    renderTarget_->draw(statText);
                }
            }

#ifdef USE_SDL2
            if (useZoomTexture) {
                // Switch to screen, blit game texture with zoom
                SDL_SetRenderTarget(ren, NULL);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                SDL_RenderClear(ren);

                int texW = static_cast<int>(m_baseViewportSize.width);
                int texH = static_cast<int>(m_baseViewportSize.height);
                int srcW = static_cast<int>(texW / m_zoomLevel);
                int srcH = static_cast<int>(texH / m_zoomLevel);
                int srcX = (texW - srcW) / 2;
                int srcY = (texH - srcH) / 2;
                SDL_Rect src = {srcX, srcY, srcW, srcH};
                SDL_Rect dst = {0, 0, texW, texH};
                SDL_RenderCopy(ren, m_gameTexture, &src, &dst);
            }
#endif
            // HUD at 1:1 on top
            drawUi();

            // Dialog AFTER UI so it renders on top of bottom panel
            if (m_currentDialog) {
#ifdef USE_SDL2
                m_currentDialog->render(renderTarget_);
#else
                m_currentDialog->render(renderWindow_);
#endif
            }

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

    if (loadingScreen->size.width > 0 && loadingScreen->size.height > 0) {
        loadingScreen->scaleX = renderTarget_->getSize().width / float(loadingScreen->size.width);
        loadingScreen->scaleY = renderTarget_->getSize().height / float(loadingScreen->size.height);
    }

    renderTarget_->draw(loadingScreen, ScreenPos(0, 0));
#ifdef USE_SDL2
    m_sdlWindow->display();
#else
    renderWindow_->display();
#endif
}

void Engine::loadTopButtons()
{
#ifdef ANDROID
    // On Android, hamburger menu replaces individual top buttons.
    // Keep only GameMenu button (hidden, triggered by hamburger).
    // Don't add visible buttons — they're too small for touch.
#else
    float x = renderTarget_->getSize().width - 5;
    for (int i=0; i<IconButton::ButtonsCount; i++) {
        std::unique_ptr<IconButton> button = std::make_unique<IconButton>(renderTarget_);

        button->setType(IconButton::Type(i));

        x -= button->rect().width;
        button->setPosition({x, 5});

        m_buttons.push_back(std::move(button));
    }
#endif
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
    {
        Size screenSize = renderTarget_->getSize();

        // Top bar background — 48px for comfortable touch targets
        renderTarget_->draw(ScreenRect(0, 0, screenSize.width, 48),
            Drawable::Color(30, 20, 10, 230));
        // Separator line
        renderTarget_->draw(ScreenRect(0, 47, screenSize.width, 1),
            Drawable::Color(80, 60, 30, 255));

        // Hamburger menu button (≡) — top right, 44x44
        {
            float bx = screenSize.width - 50;
            float by = 2;
            renderTarget_->draw(ScreenRect(bx, by, 44, 44),
                Drawable::Color(50, 35, 20, 200));
            // Three horizontal lines (hamburger icon)
            for (int i = 0; i < 3; i++) {
                renderTarget_->draw(ScreenRect(bx + 10, by + 12 + i * 8, 24, 2),
                    Drawable::Color(200, 180, 140, 255));
            }
        }

        // Resource label icons (colored squares as placeholders)
        // Wood=brown, Food=red, Gold=yellow, Stone=gray
        auto drawResIcon = [&](float x, float y, Drawable::Color c) {
            renderTarget_->draw(ScreenRect(x - 18, y + 2, 14, 14), c);
        };
        drawResIcon(60, 12, Drawable::Color(139, 90, 43, 255));   // Wood
        drawResIcon(170, 12, Drawable::Color(180, 40, 40, 255));  // Food
        drawResIcon(280, 12, Drawable::Color(220, 190, 50, 255)); // Gold
        drawResIcon(390, 12, Drawable::Color(150, 150, 150, 255));// Stone

        // --- Sliding bottom panel ---
        float panelH = screenSize.height - m_gameAreaHeight;
        // Show panel when ANY unit is selected (not just when action buttons exist)
        bool shouldShow = !state_manager_.getActiveState()->unitManager()->selected().isEmpty();

        // Animate panel slide
        m_bottomPanelTargetY = shouldShow ? m_gameAreaHeight : screenSize.height;
        float slideSpeed = 15.f; // pixels per frame
        if (m_bottomPanelY < m_bottomPanelTargetY) {
            m_bottomPanelY = std::min(m_bottomPanelY + slideSpeed, m_bottomPanelTargetY);
        } else if (m_bottomPanelY > m_bottomPanelTargetY) {
            m_bottomPanelY = std::max(m_bottomPanelY - slideSpeed, m_bottomPanelTargetY);
        }

        // Draw panel background
        if (m_bottomPanelY < screenSize.height) {
            // Dark parchment background
            renderTarget_->draw(ScreenRect(0, m_bottomPanelY, screenSize.width, panelH + 20),
                Drawable::Color(35, 25, 12, 235));
            // Top edge highlight
            renderTarget_->draw(ScreenRect(0, m_bottomPanelY, screenSize.width, 2),
                Drawable::Color(90, 70, 35, 255));
            // Subtle inner shadow
            renderTarget_->draw(ScreenRect(0, m_bottomPanelY + 2, screenSize.width, 1),
                Drawable::Color(60, 45, 20, 200));
        }

        // Draw SLP overlay on top if available
        if (m_uiOverlay && m_uiOverlay->isValid() && m_bottomPanelY < screenSize.height) {
            float scaleX = screenSize.width / m_uiOverlay->size.width;
            m_uiOverlay->scaleX = scaleX;
            m_uiOverlay->scaleY = scaleX;
            float overlayH = m_uiOverlay->size.height * scaleX;
            renderTarget_->draw(m_uiOverlay, ScreenPos(0, screenSize.height - overlayH));
        }
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

    // Help text tooltip (shown 3 seconds after button press)
    if (!m_actionPanel->lastHelpText.empty() &&
        Engine::currentTimeMs() - m_actionPanel->lastHelpTextTime < 3000) {
        auto helpText = renderTarget_->createText(Drawable::Text::Plain);
        std::string cleaned = m_actionPanel->lastHelpText;
        // Strip HTML tags from language.dll strings
        cleaned = util::stringReplace(cleaned, "<b>", "");
        cleaned = util::stringReplace(cleaned, "</b>", "");
        cleaned = util::stringReplace(cleaned, "<B>", "");
        cleaned = util::stringReplace(cleaned, "</B>", "");
        cleaned = util::stringReplace(cleaned, "<i>", "");
        cleaned = util::stringReplace(cleaned, "</i>", "");
        cleaned = util::stringReplace(cleaned, "\\n", " ");
        helpText->string = cleaned;
        helpText->pointSize = 13;
        helpText->color = Drawable::Color(255, 240, 180, 230);
        // Position above the action panel
        ScreenRect apRect = m_actionPanel->rect();
        helpText->position = ScreenPos(apRect.x, apRect.y - 22);
        // Background
        Size ts = helpText->size();
        renderTarget_->draw(ScreenRect(apRect.x - 4, apRect.y - 26, ts.width + 8, 22),
            Drawable::Color(20, 15, 8, 220));
        renderTarget_->draw(helpText);
    }

    m_woodLabel->render();
    m_foodLabel->render();
    m_goldLabel->render();
    m_stoneLabel->render();
    m_populationLabel->render();

#ifdef ANDROID
    // Age indicator + game clock in top bar
    {
        Size ss = renderTarget_->getSize();
        const Player::Ptr &human = state_manager_.getActiveState()->humanPlayer();
        if (human) {
            // Age name
            static const char* ageNames[] = {"Dark Age", "Feudal Age", "Castle Age", "Imperial Age"};
            int ageIdx = std::clamp(int(human->currentAge()), 0, 3);

            m_ageText->string = ageNames[ageIdx];
            m_ageText->position = ScreenPos(ss.width - 200, 15);
            renderTarget_->draw(m_ageText);
        }

        // Game clock
        static Time gameStartTime = Engine::currentTimeMs();
        Time elapsed = Engine::currentTimeMs() - gameStartTime;
        int secs = (elapsed / 1000) % 60;
        int mins = (elapsed / 60000) % 60;
        int hrs = elapsed / 3600000;

        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d:%02d", hrs, mins, secs);
        m_clockText->string = buf;
        m_clockText->position = ScreenPos(ss.width - 280, 15);
        renderTarget_->draw(m_clockText);

        // Score
        if (human) {
            m_scoreText->string = "Score: " + std::to_string(human->score());
            m_scoreText->position = ScreenPos(ss.width - 280, 30);
            renderTarget_->draw(m_scoreText);
        }
    }
#endif

    // Overlay screens
    if (m_diplomacyScreen) m_diplomacyScreen->render();
    if (m_settingsScreen) m_settingsScreen->render();
    if (m_techTreeScreen) m_techTreeScreen->render();

    // Game speed / pause indicator
    if (m_paused) {
        fps_label_->string = "PAUSED";
    } else if (m_gameSpeed != 1.0f) {
        fps_label_->string += " " + std::to_string(m_gameSpeed).substr(0, 3) + "x";
    }
    renderTarget_->draw(fps_label_);

    // Context menu (long-press)
    if (m_contextMenu.visible && !m_contextMenu.items.empty()) {
        float menuX = m_contextMenu.position.x;
        float menuY = m_contextMenu.position.y;
        float totalH = m_contextMenu.items.size() * ContextMenu::ITEM_HEIGHT;

        // Clamp to screen
        Size ss = renderTarget_->getSize();
        if (menuX + ContextMenu::ITEM_WIDTH > ss.width) menuX = ss.width - ContextMenu::ITEM_WIDTH - 5;
        if (menuY + totalH > ss.height) menuY = ss.height - totalH - 5;
        if (menuX < 5) menuX = 5;
        if (menuY < 50) menuY = 50;

        // Background
        renderTarget_->draw(ScreenRect(menuX - 2, menuY - 2, ContextMenu::ITEM_WIDTH + 4, totalH + 4),
            Drawable::Color(20, 15, 8, 240));
        // Border
        renderTarget_->draw(ScreenRect(menuX - 2, menuY - 2, ContextMenu::ITEM_WIDTH + 4, totalH + 4),
            Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

        for (size_t i = 0; i < m_contextMenu.items.size(); i++) {
            float iy = menuY + i * ContextMenu::ITEM_HEIGHT;
            // Item background
            renderTarget_->draw(ScreenRect(menuX, iy, ContextMenu::ITEM_WIDTH, ContextMenu::ITEM_HEIGHT - 2),
                Drawable::Color(45, 35, 18, 220));
            // Item text
            if (!fps_label_) continue; // reuse fps_label font as temp
            auto itemText = renderTarget_->createText(Drawable::Text::Plain);
            itemText->string = m_contextMenu.items[i].label;
            itemText->pointSize = 16;
            itemText->color = Drawable::Color(220, 200, 160, 255);
            itemText->position = ScreenPos(menuX + 12, iy + 12);
            renderTarget_->draw(itemText);
        }
    }

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
        } else if (choice == Dialog::Save) {
            // Save game
            MapPos camPos = renderTarget_->camera()->targetPosition();
            std::string savePath;
#ifdef ANDROID
            const char *ext = SDL_AndroidGetExternalStoragePath();
            savePath = ext ? std::string(ext) + "/save.faoe" : "/sdcard/save.faoe";
#else
            savePath = "save.faoe";
#endif
            if (SaveGame::save(savePath, *state, camPos.x, camPos.y)) {
                addMessage("Game saved!");
            } else {
                addMessage("Save failed!");
            }
            m_currentDialog.reset();
        }

        return true;
    }

    if (m_diplomacyScreen && m_diplomacyScreen->isVisible()) {
        return m_diplomacyScreen->handleEvent(event);
    }
    if (m_settingsScreen && m_settingsScreen->isVisible()) {
        return m_settingsScreen->handleEvent(event);
    }
    if (m_techTreeScreen && m_techTreeScreen->isVisible()) {
        return m_techTreeScreen->handleEvent(event);
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
        m_touchState.pinching = true;
        m_zoomLevel = std::clamp(m_zoomLevel + event.pinch.dDist * PINCH_SENSITIVITY, ZOOM_MIN, ZOOM_MAX);
        // Don't change camera viewport — SDL_RenderCopy crop handles visual zoom.
        // Changing viewport shifts absoluteScreenPos center, mismatching the crop center.
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

    // Game speed
    case input::Key::F3:
        m_paused = !m_paused;
        addMessage(m_paused ? "Game Paused" : "Game Resumed");
        return true;

    // Unit commands
    case input::Key::S: // Stop
        for (const Unit::Ptr &unit : state->unitManager()->selected()) {
            unit->actions.clearActionQueue();
        }
        return true;
    case input::Key::Delete: // Delete selected units
        for (const Unit::Ptr &unit : state->unitManager()->selected()) {
            unit->kill();
        }
        return true;
    case input::Key::Escape:
        showMenu();
        return true;

    // F1 = cycle idle villagers
    case input::Key::F1: {
        const Player::Ptr &human = state->humanPlayer();
        if (human) {
            static int lastIdleIdx = -1;
            int found = 0;
            int startIdx = lastIdleIdx + 1;
            const auto &allUnits = state->unitManager()->units();
            for (size_t i = 0; i < allUnits.size(); i++) {
                int idx = (startIdx + i) % allUnits.size();
                const Unit::Ptr &unit = allUnits[idx];
                if (!unit || unit->playerId() != human->playerId) continue;
                // Check if civilian and idle
                // Check if civilian unit (not building, not siege)
                if (unit->data()->Type < genie::Unit::CombatantType) continue;
                if (unit->data()->Speed <= 0) continue; // skip buildings
                {
                    if (!unit->actions.currentAction()) {
                        // Found idle villager
                        lastIdleIdx = idx;
                        ScreenRect selectRect(ScreenPos(-1,-1), ScreenPos(1,1));
                        state->unitManager()->selectUnits(
                            ScreenRect(ScreenPos(-999,-999), ScreenPos(999,999)),
                            renderTarget_->camera()); // deselect all first
                        renderTarget_->camera()->setTargetPosition(unit->position());
                        addMessage("Idle villager found");
                        found = 1;
                        break;
                    }
                }
            }
            if (!found) addMessage("No idle villagers");
        }
        return true;
    }

    // Control groups (Ctrl+1..9) or taunts (1..9)
    case input::Key::Num0: case input::Key::Num1: case input::Key::Num2: case input::Key::Num3:
    case input::Key::Num4: case input::Key::Num5: case input::Key::Num6:
    case input::Key::Num7: case input::Key::Num8: case input::Key::Num9: {
        int groupNum = static_cast<int>(event.key.code) - static_cast<int>(input::Key::Num0);
        if (event.key.control) {
            // Ctrl+N: assign current selection to group N
            m_controlGroups[groupNum] = state->unitManager()->selected().units;
            addMessage("Group " + std::to_string(groupNum) + " set (" + std::to_string(m_controlGroups[groupNum].size()) + " units)");
        } else {
            // N: recall group N (if assigned), otherwise play taunt
            if (!m_controlGroups[groupNum].empty()) {
                // Remove dead units
                m_controlGroups[groupNum].erase(
                    std::remove_if(m_controlGroups[groupNum].begin(), m_controlGroups[groupNum].end(),
                        [](const std::shared_ptr<Unit> &u) { return !u || u->isDead() || u->isDying(); }),
                    m_controlGroups[groupNum].end());
                if (!m_controlGroups[groupNum].empty()) {
                    state->unitManager()->setSelectedUnits(m_controlGroups[groupNum]);
                    return true;
                }
            }
            // No group assigned — play taunt
            if (groupNum >= 1 && groupNum <= 9) {
                std::string tauntFile = "taunt/taunt" + std::to_string(groupNum) + ".mp3";
                AudioPlayer::instance().playStream(tauntFile);
                addMessage("Taunt " + std::to_string(groupNum));
            }
        }
        return true;
    }

    // Cheat: F10 = +1000 all resources
    case input::Key::F10: {
        const Player::Ptr &human = state->humanPlayer();
        if (human) {
            human->setAvailableResource(genie::ResourceType::WoodStorage,
                human->resourcesAvailable(genie::ResourceType::WoodStorage) + 1000);
            human->setAvailableResource(genie::ResourceType::FoodStorage,
                human->resourcesAvailable(genie::ResourceType::FoodStorage) + 1000);
            human->setAvailableResource(genie::ResourceType::GoldStorage,
                human->resourcesAvailable(genie::ResourceType::GoldStorage) + 1000);
            human->setAvailableResource(genie::ResourceType::StoneStorage,
                human->resourcesAvailable(genie::ResourceType::StoneStorage) + 1000);
            addMessage("Cheat: +1000 all resources");
        }
        return true;
    }

    // H = center on Town Center
    case input::Key::H: {
        const Player::Ptr &human = state->humanPlayer();
        if (human) {
            // Find TC (unit type 109 = TownCenter)
            for (const Unit::Ptr &unit : state->unitManager()->units()) {
                if (unit->playerId() == human->playerId && unit->data()->ID == 109) {
                    renderTarget_->camera()->setTargetPosition(unit->position());
                    break;
                }
            }
        }
        return true;
    }

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

    // On Android, touch state machine handles drag/scroll — mouse events don't fire

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
    const ScreenPos pos(static_cast<float>(event.touch.x), static_cast<float>(event.touch.y));

    switch (event.type) {

    case input::Event::TouchBegan: {
        m_touchState.startPos = pos;
        m_touchState.lastPos = pos;
        m_touchState.startTime = currentTimeMs();
        m_touchState.pinching = false;

        if (m_touchState.phase == TouchState::Phase::WaitSecondTap) {
            // Second finger down within double-tap window — stay in WaitSecondTap,
            // will be resolved in TouchEnded
        }
        m_touchState.phase = TouchState::Phase::Pending;
        return true;
    }

    case input::Event::TouchMoved: {
        if (m_touchState.pinching) {
            m_touchState.lastPos = pos;
            return true;
        }
        if (m_touchState.phase == TouchState::Phase::Pending) {
            if (m_touchState.startPos.distanceTo(pos) > TouchState::DRAG_THRESHOLD) {
                m_touchState.phase = TouchState::Phase::Dragging;
            }
        }
        if (m_touchState.phase == TouchState::Phase::Dragging) {
            ScreenPos delta = m_touchState.lastPos - pos;
            ScreenPos camScreen = renderTarget_->camera()->targetPosition().toScreen();
            camScreen.x += delta.x;
            camScreen.y -= delta.y;
            MapPos camMap = camScreen.toMap().clamped(state->map()->pixelSize());
            renderTarget_->camera()->setTargetPosition(camMap);
        }
        m_touchState.lastPos = pos;
        return true;
    }

    case input::Event::TouchEnded: {
        // Context menu handling — if visible, check if tap hit an item
        if (m_contextMenu.visible) {
            float menuX = m_contextMenu.position.x;
            float menuY = m_contextMenu.position.y;
            Size ss = renderTarget_->getSize();
            float totalH = m_contextMenu.items.size() * ContextMenu::ITEM_HEIGHT;
            if (menuX + ContextMenu::ITEM_WIDTH > ss.width) menuX = ss.width - ContextMenu::ITEM_WIDTH - 5;
            if (menuY + totalH > ss.height) menuY = ss.height - totalH - 5;
            if (menuX < 5) menuX = 5;
            if (menuY < 50) menuY = 50;

            ScreenRect menuRect(menuX, menuY, ContextMenu::ITEM_WIDTH, totalH);
            if (menuRect.contains(pos)) {
                int idx = static_cast<int>((pos.y - menuY) / ContextMenu::ITEM_HEIGHT);
                if (idx >= 0 && idx < static_cast<int>(m_contextMenu.items.size())) {
                    int action = m_contextMenu.items[idx].action;
                    switch (action) {
                    case 0: // Move — do nothing, next tap will be move target
                        break;
                    case 1: // Attack
                        state->unitManager()->selectAttackTarget();
                        break;
                    case 2: // Patrol
                        state->unitManager()->selectPatrolTarget();
                        break;
                    case 3: // Guard
                        state->unitManager()->selectGuardTarget();
                        break;
                    case 4: // Stop
                        for (const Unit::Ptr &unit : state->unitManager()->selected()) {
                            unit->actions.clearActionQueue();
                        }
                        break;
                    case 5: // Delete
                        for (const Unit::Ptr &unit : state->unitManager()->selected()) {
                            unit->kill();
                        }
                        break;
                    }
                }
            }
            m_contextMenu.visible = false;
            m_touchState.phase = TouchState::Phase::Idle;
            return true;
        }

        // Long-press release — just dismiss (menu already shown)
        if (m_touchState.phase == TouchState::Phase::LongPress) {
            m_touchState.phase = TouchState::Phase::Idle;
            return true;
        }

        if (m_touchState.phase == TouchState::Phase::Dragging) {
            m_touchState.phase = TouchState::Phase::Idle;
            m_touchState.pinching = false;
            return true;
        }

        // It was a tap (Pending, not dragged)
        m_touchState.pinching = false;
        int64_t now = currentTimeMs();

        // --- UI elements first ---

#ifdef ANDROID
        // Hamburger menu button (top-right, 44x44 at x=screenWidth-50)
        {
            Size screenSize = renderTarget_->getSize();
            ScreenRect hamburgerRect(screenSize.width - 50, 2, 44, 44);
            if (hamburgerRect.contains(pos)) {
                showMenu();
                m_touchState.phase = TouchState::Phase::Idle;
                return true;
            }
        }
#endif

        // Top bar buttons
        IconButton::Type clickedButton = IconButton::Invalid;
        for (const std::unique_ptr<IconButton> &button : m_buttons) {
            if (button->rect().contains(pos) || ScreenPos(button->rect().center()).distanceTo(pos) < 50.f) {
                button->onMousePressed(pos);
                if (button->onMouseReleased(pos)) {
                    clickedButton = button->type();
                }
            }
        }
        if (clickedButton == IconButton::GameMenu) {
            showMenu();
        } else if (clickedButton == IconButton::Diplo) {
            if (m_diplomacyScreen) m_diplomacyScreen->show(state_manager_.getActiveState());
        } else if (clickedButton == IconButton::Chat) {
            addMessage("Chat: not yet implemented");
        } else if (clickedButton == IconButton::TechTree) {
            if (m_techTreeScreen) m_techTreeScreen->show(state_manager_.getActiveState());
        } else if (clickedButton == IconButton::Settings) {
            if (m_settingsScreen) m_settingsScreen->show();
        }
        if (clickedButton != IconButton::Invalid) {
            m_touchState.phase = TouchState::Phase::Idle;
            return true;
        }

        // Action panel
        if (m_actionPanel->handleEvent(input::Event{input::Event::MouseButtonPressed, {}, {input::MouseButton::Left, (int)pos.x, (int)pos.y}})) {
            m_actionPanel->handleEvent(input::Event{input::Event::MouseButtonReleased, {}, {input::MouseButton::Left, (int)pos.x, (int)pos.y}});
            m_touchState.phase = TouchState::Phase::Idle;
            return true;
        }

        // Minimap
        if (m_minimap->handleEvent(input::Event{input::Event::MouseButtonPressed, {}, {input::MouseButton::Left, (int)pos.x, (int)pos.y}})) {
            m_minimap->handleEvent(input::Event{input::Event::MouseButtonReleased, {}, {input::MouseButton::Left, (int)pos.x, (int)pos.y}});
            m_touchState.phase = TouchState::Phase::Idle;
            return true;
        }

        // --- Game area tap ---

        // Double-tap detection: was WaitSecondTap and second tap is close enough?
        bool isDoubleTap = (m_touchState.tapTime > 0)
            && (now - m_touchState.tapTime < TouchState::DOUBLE_TAP_MS)
            && (m_touchState.tapPos.distanceTo(pos) < TouchState::DOUBLE_TAP_DIST);

        if (isDoubleTap) {
            // Double-tap on unit = select all visible units of SAME TYPE owned by SAME PLAYER
            Unit::Ptr tappedUnit = state->unitManager()->unitAt(pos, renderTarget_->camera(), NoAlignment);
            if (tappedUnit && tappedUnit->data()) {
                Size ss = renderTarget_->getSize();
                ScreenRect gameArea(ScreenPos(0, 0), ScreenPos(ss.width, m_gameAreaHeight));
                state->unitManager()->selectUnitsByType(
                    tappedUnit->data()->ID, tappedUnit->playerId(),
                    gameArea, renderTarget_->camera());
            }
            m_touchState.phase = TouchState::Phase::Idle;
            m_touchState.tapTime = 0;
            return true;
        }

        // Single tap — try to select unit, then wait for potential second tap
        bool hasUnitAtTap = state->unitManager()->unitAt(pos, renderTarget_->camera(), NoAlignment) != nullptr;

        // Tap on empty ground with units selected → instant deselect
        if (!hasUnitAtTap && !state->unitManager()->selected().isEmpty()) {
            ScreenRect emptyRect(ScreenPos(-100, -100), ScreenPos(-99, -99));
            state->unitManager()->selectUnits(emptyRect, renderTarget_->camera());
            m_touchState.phase = TouchState::Phase::Idle;
            m_touchState.tapTime = 0;
            return true;
        }

        if (hasUnitAtTap || state->unitManager()->selected().isEmpty()) {
            ScreenRect tapRect(pos - ScreenPos(15, 15), pos + ScreenPos(15, 15));
            state->unitManager()->selectUnits(tapRect, renderTarget_->camera());
        }
        m_touchState.tapPos = pos;
        m_touchState.tapTime = now;
        m_touchState.phase = TouchState::Phase::WaitSecondTap;
        return true;
    }

    default:
        return false;
    }
}

bool Engine::handleMouseRelease(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    const ScreenPos mousePos(event.mouseButton.x, event.mouseButton.y);
    // On Android, touch state machine handles all input — mouse events don't fire
    // Buttons handled on desktop only
    IconButton::Type clickedButton = IconButton::Invalid;
    for (const std::unique_ptr<IconButton> &button : m_buttons) {
        if (button->onMouseReleased(mousePos)) {
            clickedButton = button->type();
        }
    }
    if (clickedButton == IconButton::GameMenu) {
        showMenu();
    } else if (clickedButton == IconButton::Diplo) {
        addMessage("Diplomacy: not yet implemented");
    } else if (clickedButton == IconButton::Chat) {
        addMessage("Chat: not yet implemented");
    } else if (clickedButton == IconButton::TechTree) {
        addMessage("Tech Tree: not yet implemented");
    } else if (clickedButton == IconButton::Settings) {
        addMessage("Settings: not yet implemented");
    }
    if (clickedButton != IconButton::Invalid) {
        return true;
    }

    // Game area release
    if (mousePos.y < m_gameAreaHeight && event.mouseButton.button == input::MouseButton::Left) {
        if (state->unitManager()->onMouseRelease()) {
            return true;
        }
    }

    if (event.mouseButton.button == input::MouseButton::Left && m_selecting) {
        ScreenRect selectRect(m_selectionStart, mousePos);
        if (selectRect.width < 15 && selectRect.height < 15) {
            selectRect = ScreenRect(mousePos - ScreenPos(15, 15), mousePos + ScreenPos(15, 15));
        }
        state->unitManager()->selectUnits(selectRect, renderTarget_->camera());
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
    // Must be set BEFORE SDL_Init / window creation
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"); // handle touch via state machine, no mouse synthesis
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0"); // don't generate touch from mouse
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
    m_mainScreen->setRenderTarget(renderTarget_);
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

    // Cache HUD text objects (avoid createText per frame)
    m_ageText = renderTarget_->createText(Drawable::Text::Plain);
    m_ageText->pointSize = 13;
    m_ageText->color = Drawable::Color(180, 160, 120, 255);
    m_clockText = renderTarget_->createText(Drawable::Text::Plain);
    m_clockText->pointSize = 12;
    m_clockText->color = Drawable::Color(150, 140, 110, 255);
    m_scoreText = renderTarget_->createText(Drawable::Text::Plain);
    m_scoreText->pointSize = 12;
    m_scoreText->color = Drawable::Color(150, 140, 110, 255);

#ifdef ANDROID
    // Mobile layout: taller top bar with more spacing
    m_woodLabel->setPosition({60, 12});
    m_foodLabel->setPosition({170, 12});
    m_goldLabel->setPosition({280, 12});
    m_stoneLabel->setPosition({390, 12});
    m_populationLabel->setPosition({500, 12});
#else
    m_woodLabel->setPosition({75, 5});
    m_foodLabel->setPosition({153, 5});
    m_goldLabel->setPosition({230, 5});
    m_stoneLabel->setPosition({307, 5});
    m_populationLabel->setPosition({384, 5});
#endif

    showStartScreen();

    std::shared_ptr<GameState> gameState = std::make_shared<GameState>(renderTarget_);
    if (scenario) {
        gameState->setScenario(scenario);
    }
    if (m_skipDemoGame) {
        gameState->setSkipDemoGame(true);
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

    m_diplomacyScreen = std::make_unique<DiplomacyScreen>(renderTarget_);
    m_settingsScreen = std::make_unique<SettingsScreen>(renderTarget_);
    m_techTreeScreen = std::make_unique<TechTreeScreen>(renderTarget_);

    m_mapRenderer = std::make_unique<MapRenderer>();
    m_mapRenderer->setRenderTarget(renderTarget_);

    loadUiOverlay();

    Size uiSize = (m_uiOverlay && m_uiOverlay->isValid()) ? m_uiOverlay->size : Size(1280, 1024);
    if (m_uiOverlay && m_uiOverlay->size.isValid()) {
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
    // Reset logical size that ScenarioBrowser may have set — otherwise mouse coords mismatch
    SDL_RenderSetLogicalSize(
        static_cast<SdlRenderTarget*>(renderTarget_.get())->renderer(),
        0, 0
    );
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

#ifdef ANDROID
    // On Android, bottom ~35% for UI panel (slides up when units selected)
    m_gameAreaHeight = uiSize.height * 0.65f;
    m_bottomPanelY = uiSize.height; // start hidden below screen
#else
    // Calculate game area height (screen minus UI overlay)
    // m_uiOverlayOffset is where the overlay image starts (top of the bottom bar)
    if (m_uiOverlay && m_uiOverlay->size.isValid()) {
        m_gameAreaHeight = m_uiOverlayOffset > 0 ? m_uiOverlayOffset : uiSize.height * 0.75f;
    } else {
        m_gameAreaHeight = uiSize.height * 0.75f;
    }
#endif

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
        addMessage("Menu: dlg_men.sin not found");
        return;
    }

    genie::SlpFilePtr backgroundSlp = AssetManager::Inst()->getSlp(uiFile->backgroundSmall.fileId);
    if (!backgroundSlp) {
        WARN << "Failed to load menu background";
        addMessage("Menu: background image not found");
        return;
    }
    auto frame = backgroundSlp->getFrame(0);
    m_currentDialog = std::make_unique<Dialog>(m_mainScreen.get());
    if (frame) {
        Resource::RawImage menuBg = Resource::convertFrameToImage(frame);
        m_currentDialog->background = renderTarget_->createImage(Size(menuBg.width, menuBg.height), menuBg.pixels.data());
    }
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
