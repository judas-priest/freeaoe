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
#include "audio/NotificationManager.h"
#include "ui/DiplomacyScreen.h"
#include "ui/LobbyScreen.h"
#include "ui/SettingsScreen.h"
#include "ui/TechTreeScreen.h"
#include "mechanics/SaveGame.h"

#include "core/Logger.h"
#include "core/ResourceMap.h"
#include "mechanics/GameState.h"
#include "mechanics/Map.h"
#include "mechanics/MapTile.h"
#include "mechanics/Player.h"
#include "mechanics/ScenarioController.h"
#include "mechanics/UnitManager.h"
#include "global/EventManager.h"
#include "net/NetHost.h"
#include "net/NetClient.h"
#include "net/LockstepManager.h"
#include <genie/script/ScnFile.h>

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
#include <ctime>
#include <cstdlib>

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
void Engine::setupRandomMap(int mapType, int mapSize, int playerCount,
                            int startingAge, const int *civIds, const int *teams)
{
    auto state = state_manager_.getActiveState();
    if (!state) return;

    // Two-phase random map setup:
    // Phase 1: create players and terrain (no units yet)
    // Phase 2: wire up renderers, then place units (which trigger visibility events)

    state->setupRandomMap(mapType, mapSize, playerCount, startingAge, civIds, teams);

    // Wire up renderers after map and players exist
    if (state->humanPlayer()) {
        m_minimap->setMap(state->map());
        m_minimap->setVisibilityMap(state->humanPlayer()->visibility);
        m_minimap->setHumanPlayer(state->humanPlayer());
        m_minimap->setAllPlayers(state->players());
        m_mapRenderer->setVisibilityMap(state->humanPlayer()->visibility);
        m_mapRenderer->setHumanPlayer(state->humanPlayer());
        m_mapRenderer->setAllPlayers(state->players());
        m_mapRenderer->setMap(state->map());
        m_actionPanel->setUnitManager(state->unitManager());
        m_actionPanel->setHumanPlayer(state->humanPlayer());
        m_unitInfoPanel->setUnitManager(state->unitManager());
        m_unitsRenderer->setUnitManager(state->unitManager());
        m_unitsRenderer->setVisibilityMap(state->humanPlayer()->visibility);

        // Initialize voice notification manager for random maps
        if (!m_notificationManager) {
            m_notificationManager = std::make_unique<NotificationManager>();
        }
        m_notificationManager->setHumanPlayerId(state->humanPlayer()->playerId);
    }
}

void Engine::setupMultiplayerHost(uint16_t port)
{
    NetSocket::initNetworking();

    m_netHost = std::make_shared<NetHost>();
    if (!m_netHost->start(port)) {
        WARN << "Failed to start multiplayer host on port" << port;
        m_netHost.reset();
        return;
    }

    m_lockstep = std::make_shared<LockstepManager>();
    m_lockstep->setHost(m_netHost);
    m_lockstep->setLocalPlayerId(0); // Host is player 0

    auto state = state_manager_.getActiveState();
    if (state) {
        state->setLockstepManager(m_lockstep);
    }

    DBG << "Multiplayer host started on port" << port;
}

void Engine::setupMultiplayerClient(const std::string &host, uint16_t port)
{
    NetSocket::initNetworking();

    m_netClient = std::make_shared<NetClient>();
    if (!m_netClient->connect(host, port)) {
        WARN << "Failed to connect to" << host << ":" << port;
        m_netClient.reset();
        return;
    }

    // Receive the welcome message with our assigned player ID
    // Poll briefly for the initial LobbyJoin response
    for (int attempt = 0; attempt < 100 && m_netClient->assignedPlayerId() < 0; attempt++) {
        m_netClient->update();
        if (m_netClient->assignedPlayerId() >= 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    m_lockstep = std::make_shared<LockstepManager>();
    m_lockstep->setClient(m_netClient);
    m_lockstep->setLocalPlayerId(m_netClient->assignedPlayerId());

    auto state = state_manager_.getActiveState();
    if (state) {
        state->setLockstepManager(m_lockstep);
    }

    DBG << "Multiplayer client connected, assigned player" << m_netClient->assignedPlayerId();
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
            m_minimap->setHumanPlayer(state->humanPlayer());
            m_minimap->setAllPlayers(state->players());

            m_mapRenderer->setVisibilityMap(state->humanPlayer()->visibility);
            m_mapRenderer->setHumanPlayer(state->humanPlayer());
            m_mapRenderer->setAllPlayers(state->players());
            m_mapRenderer->setMap(state->map());

            m_actionPanel->setUnitManager(state->unitManager());
            m_actionPanel->setHumanPlayer(state->humanPlayer());
            m_unitInfoPanel->setUnitManager(state->unitManager());

            m_unitsRenderer->setUnitManager(state->unitManager());
            m_unitsRenderer->setVisibilityMap(state->humanPlayer()->visibility);

            // Initialize or update voice notification manager
            if (!m_notificationManager) {
                m_notificationManager = std::make_unique<NotificationManager>();
            }
            m_notificationManager->setHumanPlayerId(state->humanPlayer()->playerId);
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

        // WaitSecondTap timeout: just reset phase, deselection handled in TouchEnded
        if (m_touchState.phase == TouchState::Phase::WaitSecondTap) {
            int64_t elapsed = currentTimeMs() - m_touchState.tapTime;
            if (elapsed >= TouchState::DOUBLE_TAP_MS) {
                m_touchState.phase = TouchState::Phase::Idle;
            }
        }

        // Long-press detection: if finger held down > 600ms = right-click (move/attack)
        if (m_touchState.phase == TouchState::Phase::Pending) {
            int64_t held = currentTimeMs() - m_touchState.startTime;
            const bool isPlacing = state->unitManager()->state() == UnitManager::State::PlacingBuilding ||
                                   state->unitManager()->state() == UnitManager::State::PlacingWall;
            if (held >= TouchState::LONG_PRESS_MS && !state->unitManager()->selected().isEmpty() && !isPlacing) {
                // Update tasks under cursor BEFORE right-click so gathering/attacking works
                state->unitManager()->onCursorPositionChanged(m_touchState.startPos, renderTarget_->camera());
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

        // Check if lobby requested game start
        if (m_lobbyScreen && m_lobbyScreen->isVisible() && m_lobbyScreen->isGameStartRequested()) {
            m_lobbyScreen->clearStartRequest();
            if (m_lobbyScreen->isHosting()) {
                setupMultiplayerHost(12345);
            } else {
                setupMultiplayerClient("127.0.0.1", 12345);
            }
            // Wire lockstep into UnitManager for command interception
            if (m_lockstep && state) {
                state->unitManager()->setMultiplayer(true);
                state->unitManager()->setLockstep(m_lockstep);
            }
            m_lobbyScreen->hide();
            addMessage("Multiplayer game started!");
        }

        // Multiplayer: handle disconnections and speed sync
        if (m_lockstep && m_lockstep->isMultiplayer()) {
            if (m_netHost) {
                auto disconnected = m_netHost->popDisconnectedPlayers();
                for (int pid : disconnected) {
                    addMessage("Player " + std::to_string(pid) + " disconnected");
                    // Mark the player as defeated
                    auto p = state->player(pid);
                    if (p) p->alive = false;
                }
            }
            if (m_netClient) {
                auto disconnected = m_netClient->popDisconnectedPlayers();
                for (int pid : disconnected) {
                    addMessage("Player " + std::to_string(pid) + " disconnected");
                    auto p = state->player(pid);
                    if (p) p->alive = false;
                }
                float newSpeed;
                if (m_netClient->popSpeedChange(newSpeed)) {
                    m_gameSpeed = newSpeed;
                    addMessage("Game speed: " + std::to_string(m_gameSpeed).substr(0, 3) + "x");
                }
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
                const Size ws = renderTarget_->getSize();

                // Fullscreen dim
                renderTarget_->draw(ScreenRect(0, 0, ws.width, ws.height),
                                    Drawable::Color(0, 0, 0, 180));

                // Panel geometry
                const int panelW = std::min(500, int(ws.width) - 40);
                const int panelH = 320;
                const int panelX = (int(ws.width)  - panelW) / 2;
                const int panelY = (int(ws.height) - panelH) / 2;

                // Panel background
                renderTarget_->draw(ScreenRect(panelX, panelY, panelW, panelH),
                                    Drawable::Color(60, 50, 40, 220));
                // Panel border
                const Drawable::Color border(160, 140, 100, 255);
                renderTarget_->draw(ScreenRect(panelX,          panelY,          panelW, 2), border);
                renderTarget_->draw(ScreenRect(panelX,          panelY+panelH-2, panelW, 2), border);
                renderTarget_->draw(ScreenRect(panelX,          panelY,          2, panelH), border);
                renderTarget_->draw(ScreenRect(panelX+panelW-2, panelY,          2, panelH), border);

                // Title
                m_resultOverlay->position = ScreenPos(ws.width / 2, panelY + 36);
                renderTarget_->draw(m_resultOverlay);

                // Stats
                const Player::Ptr &human = state->humanPlayer();
                if (human) {
                    const int statX  = panelX + 24;
                    const int valueX = panelX + panelW - 24;
                    float sy = panelY + 76;
                    m_statText->color = Drawable::Color(200, 190, 150, 255);

                    auto drawStat = [&](const std::string &label, int value) {
                        m_statText->string = label;
                        m_statText->position = ScreenPos(statX, sy);
                        renderTarget_->draw(m_statText);
                        m_statText->string = std::to_string(value);
                        m_statText->position = ScreenPos(valueX - int(m_statText->string.size()) * 9, sy);
                        renderTarget_->draw(m_statText);
                        sy += 26;
                    };

                    drawStat("Score",             human->score());
                    drawStat("Units Killed",      human->unitsKilled);
                    drawStat("Units Lost",        human->unitsLost);
                    drawStat("Buildings Razed",   human->buildingsRazed);
                    drawStat("Techs Researched",  human->techsResearched);

                    // Buttons — simple filled rects with centered text
                    const int btnW = panelW - 48;
                    const int btnH = 52;
                    const int btnX = panelX + 24;
                    int btnY = panelY + panelH - (m_showContinueButton ? 130 : 74);

                    auto drawButton = [&](TextButton &btn, int bx, int by) {
                        btn.rect = ScreenRect(bx, by, btnW, btnH);
                        Drawable::Color bg = btn.pressed
                            ? Drawable::Color(80, 70, 50, 255)
                            : Drawable::Color(40, 35, 25, 255);
                        renderTarget_->draw(btn.rect, bg);
                        renderTarget_->draw(ScreenRect(bx, by, btnW, 1), border);
                        renderTarget_->draw(ScreenRect(bx, by+btnH-1, btnW, 1), border);
                        renderTarget_->draw(ScreenRect(bx, by, 1, btnH), border);
                        renderTarget_->draw(ScreenRect(bx+btnW-1, by, 1, btnH), border);
                        m_statText->string = btn.text;
                        m_statText->color = Drawable::Color(220, 210, 180, 255);
                        m_statText->position = ScreenPos(bx + btnW/2 - int(btn.text.size()) * 4, by + btnH/2 - 7);
                        renderTarget_->draw(m_statText);
                    };

                    drawButton(m_btnReturnToMenu, btnX, btnY);

                    if (m_showContinueButton) {
                        btnY += btnH + 8;
                        // Show "Next Mission" for campaigns, "Continue Playing" for standalone
                        if (!m_campaignPath.empty() && m_campaignScenarioIndex >= 0 &&
                            m_campaignScenarioIndex + 1 < m_campaignScenarioCount) {
                            m_btnContinuePlaying.text = "Next Mission";
                        } else {
                            m_btnContinuePlaying.text = "Continue Playing";
                        }
                        drawButton(m_btnContinuePlaying, btnX, btnY);
                    }
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

void Engine::clearMessages()
{
    for (int i = 0; i < s_numMessagesLines; i++) {
        m_visibleText[i].text->string = "";
        m_visibleText[i].endTime = 0;
    }
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

    // Minimap mode label (Task 7)
    if (m_minimapModeText) {
        ScreenRect mmRect = m_minimap->rect();
        const char *modeStr = "Normal";
        switch (m_minimap->mode()) {
        case Minimap::MinimapMode::Diplomatic: modeStr = "Diplo"; break;
        case Minimap::MinimapMode::Economic:   modeStr = "Econ";  break;
        case Minimap::MinimapMode::Normal:     modeStr = "Normal"; break;
        }
        m_minimapModeText->string = modeStr;
        m_minimapModeText->position = ScreenPos(mmRect.x + mmRect.width / 2 - 14, mmRect.y + mmRect.height + 2);
        renderTarget_->draw(m_minimapModeText);
    }

    m_actionPanel->draw();
    m_unitInfoPanel->draw();

    // Help text tooltip (shown 3 seconds after button press)
    if (!m_actionPanel->lastHelpText.empty() &&
        Engine::currentTimeMs() - m_actionPanel->lastHelpTextTime < 3000) {
        std::string cleaned = m_actionPanel->lastHelpText;
        // Strip HTML tags from language.dll strings
        cleaned = util::stringReplace(cleaned, "<b>", "");
        cleaned = util::stringReplace(cleaned, "</b>", "");
        cleaned = util::stringReplace(cleaned, "<B>", "");
        cleaned = util::stringReplace(cleaned, "</B>", "");
        cleaned = util::stringReplace(cleaned, "<i>", "");
        cleaned = util::stringReplace(cleaned, "</i>", "");
        cleaned = util::stringReplace(cleaned, "\\n", " ");
        m_helpText->string = cleaned;
        // Position above the action panel
        ScreenRect apRect = m_actionPanel->rect();
        m_helpText->position = ScreenPos(apRect.x, apRect.y - 22);
        // Background
        Size ts = m_helpText->size();
        renderTarget_->draw(ScreenRect(apRect.x - 4, apRect.y - 26, ts.width + 8, 22),
            Drawable::Color(20, 15, 8, 220));
        renderTarget_->draw(m_helpText);
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
    if (m_lobbyScreen) m_lobbyScreen->render();
    if (m_settingsScreen) m_settingsScreen->render();
    if (m_techTreeScreen) m_techTreeScreen->render();
    renderSaveLoadScreen();

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
            m_menuItemText->string = m_contextMenu.items[i].label;
            m_menuItemText->position = ScreenPos(menuX + 12, iy + 12);
            renderTarget_->draw(m_menuItemText);
        }
    }

    const Time currentTime = Engine::currentTimeMs();
    for (const MessageLine &messageLine : m_visibleText) {
        if (messageLine.endTime < currentTime) {
            continue;
        }
        renderTarget_->draw(messageLine.text);
    }

    // Objectives panel
    auto activeState = state_manager_.getActiveState();
    if (m_objectivesVisible && activeState && activeState->scenarioController()) {
        const auto &objs = activeState->scenarioController()->objectives();
        if (!objs.empty() && m_statText) {
            const int panelH = 24 + int(objs.size()) * 20;
            renderTarget_->draw(ScreenRect(8, 55, 300, panelH),
                                Drawable::Color(0, 0, 0, 160));
            float oy = 60;
            for (const auto &obj : objs) {
                std::string prefix = obj.completed ? "[x] " : "[ ] ";
                m_statText->string = prefix + obj.description;
                m_statText->color = obj.completed
                    ? Drawable::Color(128, 255, 128, 255)
                    : Drawable::White;
                m_statText->position = ScreenPos(14, oy);
                renderTarget_->draw(m_statText);
                oy += 20;
            }
        }
    }

    // Stats overlay (Task 5: post-game statistics screen)
    if (m_statsVisible && m_statText) {
        auto activeState2 = state_manager_.getActiveState();
        if (activeState2) {
            const Size ws = renderTarget_->getSize();
            const int panelW = std::min(520, int(ws.width) - 40);
            const int lineH = 22;
            const auto &allPlayers = activeState2->players();
            const int numPlayers = int(allPlayers.size());
            // Header + 1 row per player + column headers
            const int panelH = 60 + (numPlayers + 1) * lineH + 20;
            const int panelX = (int(ws.width) - panelW) / 2;
            const int panelY = (int(ws.height) - panelH) / 2;

            // Background
            renderTarget_->draw(ScreenRect(panelX, panelY, panelW, panelH),
                                Drawable::Color(30, 25, 15, 230));
            // Border
            const Drawable::Color statsBorder(140, 120, 80, 255);
            renderTarget_->draw(ScreenRect(panelX, panelY, panelW, 2), statsBorder);
            renderTarget_->draw(ScreenRect(panelX, panelY + panelH - 2, panelW, 2), statsBorder);
            renderTarget_->draw(ScreenRect(panelX, panelY, 2, panelH), statsBorder);
            renderTarget_->draw(ScreenRect(panelX + panelW - 2, panelY, 2, panelH), statsBorder);

            // Title
            m_statText->color = Drawable::Color(220, 200, 160, 255);
            m_statText->string = "Game Statistics";
            m_statText->position = ScreenPos(panelX + panelW / 2 - 60, panelY + 12);
            renderTarget_->draw(m_statText);

            // Column headers
            float hy = panelY + 42;
            m_statText->color = Drawable::Color(180, 170, 130, 255);
            const int col0 = panelX + 10;
            const int col1 = panelX + 120;
            const int col2 = panelX + 190;
            const int col3 = panelX + 250;
            const int col4 = panelX + 330;
            const int col5 = panelX + 410;

            m_statText->string = "Player"; m_statText->position = ScreenPos(col0, hy); renderTarget_->draw(m_statText);
            m_statText->string = "Score";  m_statText->position = ScreenPos(col1, hy); renderTarget_->draw(m_statText);
            m_statText->string = "Kills";  m_statText->position = ScreenPos(col2, hy); renderTarget_->draw(m_statText);
            m_statText->string = "Lost";   m_statText->position = ScreenPos(col3, hy); renderTarget_->draw(m_statText);
            m_statText->string = "Razed";  m_statText->position = ScreenPos(col4, hy); renderTarget_->draw(m_statText);
            m_statText->string = "Techs";  m_statText->position = ScreenPos(col5, hy); renderTarget_->draw(m_statText);

            // Separator
            renderTarget_->draw(ScreenRect(panelX + 8, int(hy) + lineH - 2, panelW - 16, 1),
                                Drawable::Color(100, 80, 50, 200));

            // Player rows
            float ry = hy + lineH;
            for (int i = 0; i < numPlayers; i++) {
                const Player::Ptr &p = allPlayers[i];
                if (!p) continue;
                bool isHuman = (p == activeState2->humanPlayer());
                m_statText->color = isHuman
                    ? Drawable::Color(255, 220, 100, 255)
                    : Drawable::Color(200, 190, 150, 255);
                m_statText->string = p->name.empty() ? ("Player " + std::to_string(i + 1)) : p->name;
                m_statText->position = ScreenPos(col0, ry); renderTarget_->draw(m_statText);
                m_statText->string = std::to_string(p->score());
                m_statText->position = ScreenPos(col1, ry); renderTarget_->draw(m_statText);
                m_statText->string = std::to_string(p->unitsKilled);
                m_statText->position = ScreenPos(col2, ry); renderTarget_->draw(m_statText);
                m_statText->string = std::to_string(p->unitsLost);
                m_statText->position = ScreenPos(col3, ry); renderTarget_->draw(m_statText);
                m_statText->string = std::to_string(p->buildingsRazed);
                m_statText->position = ScreenPos(col4, ry); renderTarget_->draw(m_statText);
                m_statText->string = std::to_string(p->techsResearched);
                m_statText->position = ScreenPos(col5, ry); renderTarget_->draw(m_statText);
                ry += lineH;
            }

            // Close hint
            m_statText->color = Drawable::Color(140, 130, 100, 200);
            m_statText->string = "Press Tab or Esc to close";
            m_statText->position = ScreenPos(panelX + panelW / 2 - 80, ry + 6);
            renderTarget_->draw(m_statText);
        }
    }

    // Chat input bar
    if (m_chat.active) {
        const Size ws = renderTarget_->getSize();
        renderTarget_->draw(ScreenRect(0, m_gameAreaHeight - 28, ws.width, 28),
                            Drawable::Color(0, 0, 0, 180));
        if (m_statText) {
            std::string targetLabel = (m_chat.target == -2) ? "[Allies] " : "[All] ";
            m_statText->string = targetLabel + "Say: " + m_chat.buffer + "_";
            m_statText->color = Drawable::White;
            m_statText->position = ScreenPos(8, m_gameAreaHeight - 24);
            renderTarget_->draw(m_statText);
        }
    }

    // Scenario briefing overlay
    if (m_showBriefing && !m_scenarioBriefing.empty() && m_statText) {
        const Size ws = renderTarget_->getSize();

        // Full screen dim
        renderTarget_->draw(ScreenRect(0, 0, ws.width, ws.height),
                            Drawable::Color(0, 0, 0, 200));

        // Panel
        const float panelW = std::min(700.f, ws.width - 40.f);
        const float panelH = std::min(500.f, ws.height - 60.f);
        const float panelX = (ws.width - panelW) / 2;
        const float panelY = (ws.height - panelH) / 2;

        renderTarget_->draw(ScreenRect(panelX, panelY, panelW, panelH),
                            Drawable::Color(30, 22, 10, 245));
        renderTarget_->draw(ScreenRect(panelX, panelY, panelW, panelH),
                            Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

        // Title
        if (m_helpText) {
            m_helpText->string = "Mission Briefing";
            m_helpText->color = Drawable::Color(255, 220, 150, 255);
            m_helpText->pointSize = 20;
            m_helpText->position = ScreenPos(panelX + 15, panelY + 10);
            renderTarget_->draw(m_helpText);
        }

        // Briefing text (wrap lines manually at panel width)
        m_statText->color = Drawable::Color(220, 210, 180, 255);
        m_statText->pointSize = 13;
        float textY = panelY + 45;
        const float maxTextY = panelY + panelH - 50;

        // Split text by newlines and display
        std::string remaining = m_scenarioBriefing;
        size_t pos = 0;
        while (pos < remaining.size() && textY < maxTextY) {
            size_t nl = remaining.find('\n', pos);
            std::string line = (nl != std::string::npos)
                ? remaining.substr(pos, nl - pos)
                : remaining.substr(pos);
            pos = (nl != std::string::npos) ? nl + 1 : remaining.size();

            // Truncate long lines to fit panel
            if (line.size() > static_cast<size_t>(panelW / 7)) {
                line = line.substr(0, static_cast<size_t>(panelW / 7)) + "...";
            }

            m_statText->string = line;
            m_statText->position = ScreenPos(panelX + 15, textY);
            renderTarget_->draw(m_statText);
            textY += 18;
        }

        // "Start Mission" button
        const float btnW = 160;
        const float btnH = 36;
        const float btnX = panelX + (panelW - btnW) / 2;
        const float btnY = panelY + panelH - 45;
        renderTarget_->draw(ScreenRect(btnX, btnY, btnW, btnH),
                            Drawable::Color(60, 45, 20, 230));
        renderTarget_->draw(ScreenRect(btnX, btnY, btnW, btnH),
                            Drawable::Transparent, Drawable::Color(120, 100, 60, 255));
        if (m_menuItemText) {
            m_menuItemText->string = "Start Mission";
            m_menuItemText->color = Drawable::Color(220, 200, 160, 255);
            m_menuItemText->position = ScreenPos(btnX + 20, btnY + 8);
            renderTarget_->draw(m_menuItemText);
        }
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
    // Scenario briefing overlay — dismiss on Escape, click, or touch
    if (m_showBriefing) {
        if (event.type == input::Event::KeyPressed && event.key.code == input::Key::Escape) {
            m_showBriefing = false;
            return true;
        }
        if (event.type == input::Event::MouseButtonReleased ||
            event.type == input::Event::TouchEnded) {
            m_showBriefing = false;
            return true;
        }
        return true; // consume all input while briefing visible
    }

    // Post-game overlay — consume all input, handle button taps
    if (state && state->result != GameState::Result::Running) {
        ScreenPos pos;
        bool isRelease = false;

        if (event.type == input::Event::MouseButtonReleased) {
            pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
            isRelease = true;
        } else if (event.type == input::Event::TouchEnded) {
            pos = ScreenPos(event.touch.x, event.touch.y);
            isRelease = true;
        } else if (event.type == input::Event::MouseButtonPressed ||
                   event.type == input::Event::TouchBegan) {
            ScreenPos p = (event.type == input::Event::MouseButtonPressed)
                ? ScreenPos(event.mouseButton.x, event.mouseButton.y)
                : ScreenPos(event.touch.x, event.touch.y);
            m_btnReturnToMenu.pressed   = m_btnReturnToMenu.rect.contains(p);
            m_btnContinuePlaying.pressed = m_btnContinuePlaying.rect.contains(p);
            return true;
        }

        if (isRelease) {
            m_btnReturnToMenu.pressed    = false;
            m_btnContinuePlaying.pressed = false;

            if (m_btnReturnToMenu.rect.contains(pos)) {
                m_sdlWindow->close();
                return true;
            }
            if (m_showContinueButton && m_btnContinuePlaying.rect.contains(pos)) {
                if (!m_campaignPath.empty() && m_campaignScenarioIndex >= 0 &&
                    m_campaignScenarioIndex + 1 < m_campaignScenarioCount) {
                    loadNextCampaignScenario();
                } else {
                    state->result = GameState::Result::Running;
                }
                return true;
            }
        }
        return true; // consume all input while overlay visible
    }

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
        } else if (choice == Dialog::Achievements) {
            // Achievements button → Load game dialog
            m_currentDialog.reset();
            showSaveLoadScreen(true);
        } else if (choice == Dialog::Save) {
            // Save button → Save game dialog
            m_currentDialog.reset();
            showSaveLoadScreen(false);
        } else if (choice == Dialog::Options) {
            m_currentDialog.reset();
            if (m_settingsScreen) m_settingsScreen->show();
        }

        return true;
    }

    if (m_saveLoadScreen.visible) {
        return handleSaveLoadEvent(event);
    }

    if (m_lobbyScreen && m_lobbyScreen->isVisible()) {
        return m_lobbyScreen->handleEvent(event);
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

    // Chat text input
    if (event.type == input::Event::TextEntered && m_chat.active) {
        uint32_t ch = event.text.unicode;
        if (ch >= 32 && ch < 127) { // printable ASCII
            m_chat.buffer += static_cast<char>(ch);
        }
        return true;
    }

    // When chat is active, only pass Enter/Escape/Backspace/Tab to key handler
    if (m_chat.active && event.type == input::Event::KeyPressed) {
        if (event.key.code == input::Key::Return ||
            event.key.code == input::Key::Escape ||
            event.key.code == input::Key::BackSpace ||
            event.key.code == input::Key::Tab) {
            return handleKeyEvent(event, state);
        }
        return true; // consume all other keys while chatting
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

static std::string quickSavePath()
{
#ifdef __ANDROID__
    const char *internal = SDL_AndroidGetInternalStoragePath();
    if (internal) {
        return std::string(internal) + "/quicksave.faoe";
    }
    return "/sdcard/quicksave.faoe";
#elif defined(USE_SDL2)
    char *prefPath = SDL_GetPrefPath("freeaoe", "freeaoe");
    std::string path;
    if (prefPath) {
        path = std::string(prefPath) + "quicksave.faoe";
        SDL_free(prefPath);
    } else {
        path = "quicksave.faoe";
    }
    return path;
#else
    return "quicksave.faoe";
#endif
}

bool Engine::handleKeyEvent(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    ScreenPos cameraScreenPos = renderTarget_->camera()->targetPosition().toScreen();

    switch(event.key.code) {
    case input::Key::Left:
        cameraScreenPos.x -= 20;
        m_followUnit.reset();
        break;
    case input::Key::Right:
        cameraScreenPos.x += 20;
        m_followUnit.reset();
        break;
    case input::Key::Down:
        cameraScreenPos.y -= 20;
        m_followUnit.reset();
        break;
    case input::Key::Up:
        cameraScreenPos.y += 20;
        m_followUnit.reset();
        break;

    // Game speed
    case input::Key::F3:
        m_paused = !m_paused;
        addMessage(m_paused ? "Game Paused" : "Game Resumed");
        return true;
    case input::Key::F4:
        if (m_minimap) {
            m_minimap->cycleMode();
        }
        return true;
    case input::Key::F5: {
        const std::string savePath = quickSavePath();
        MapPos camPos = renderTarget_->camera()->targetPosition();
        if (SaveGame::save(savePath, *state, camPos.x, camPos.y)) {
            addMessage("Game saved to " + savePath);
        } else {
            addMessage("Failed to save game!");
        }
        return true;
    }
    case input::Key::F9: {
        const std::string savePath = quickSavePath();
        float camX = 0, camY = 0;
        if (SaveGame::load(savePath, *state, camX, camY)) {
            renderTarget_->camera()->setTargetPosition(MapPos(camX, camY));
            addMessage("Game loaded from " + savePath);
        } else {
            addMessage("No save file found!");
        }
        return true;
    }
    case input::Key::F11:
        m_objectivesVisible = !m_objectivesVisible;
        return true;

    case input::Key::F12:
        // Toggle multiplayer lobby
        if (m_lobbyScreen && !m_lobbyScreen->isVisible()) {
            m_lobbyScreen->show(true); // Default to host mode; Shift+F12 for join
            addMessage("Multiplayer lobby opened (hosting). Shift+F12 to join.");
        } else if (m_lobbyScreen) {
            m_lobbyScreen->hide();
        }
        return true;

    case input::Key::F6: {
        // Toggle camera follow on selected unit
        if (auto existing = m_followUnit.lock()) {
            m_followUnit.reset();
            addMessage("Camera follow OFF");
        } else {
            const auto &sel = state->unitManager()->selected();
            if (!sel.isEmpty()) {
                m_followUnit = sel.units.front();
                addMessage("Camera following unit");
            } else {
                addMessage("No unit selected");
            }
        }
        return true;
    }

    case input::Key::F7: {
        // Slow down game speed (only host can change speed in multiplayer)
        if (m_lockstep && m_lockstep->isMultiplayer() && !m_lockstep->isHost()) {
            addMessage("Only the host can change game speed");
            return true;
        }
        m_gameSpeed = std::max(0.5f, m_gameSpeed - 0.5f);
        addMessage("Game speed: " + std::to_string(m_gameSpeed).substr(0, 3) + "x");
        if (m_netHost) m_netHost->broadcastSpeedChange(m_gameSpeed);
        return true;
    }
    case input::Key::F8: {
        // Speed up game speed (only host can change speed in multiplayer)
        if (m_lockstep && m_lockstep->isMultiplayer() && !m_lockstep->isHost()) {
            addMessage("Only the host can change game speed");
            return true;
        }
        m_gameSpeed = std::min(3.0f, m_gameSpeed + 0.5f);
        addMessage("Game speed: " + std::to_string(m_gameSpeed).substr(0, 3) + "x");
        if (m_netHost) m_netHost->broadcastSpeedChange(m_gameSpeed);
        return true;
    }

    // Unit commands
    case input::Key::S: // Stop
        for (const Unit::Ptr &unit : state->unitManager()->selected()) {
            unit->actions.clearActionQueue();
        }
        return true;
    case input::Key::A:
        if (event.key.shift) {
            // Shift+A: select all visible military units
            const Player::Ptr &human = state->humanPlayer();
            if (human) {
                UnitVector military;
                for (const Unit::Ptr &unit : state->unitManager()->units()) {
                    if (!unit || unit->isDead() || unit->isDying()) continue;
                    if (unit->playerId() != human->playerId) continue;
                    if (unit->isBuilding()) continue;
                    if (unit->data()->Class == genie::Unit::Civilian) continue;
                    if (unit->data()->Speed <= 0) continue;
                    military.push_back(unit);
                }
                if (!military.empty()) {
                    state->unitManager()->setSelectedUnits(military);
                    addMessage("Selected " + std::to_string(military.size()) + " military units");
                } else {
                    addMessage("No military units");
                }
            }
        } else {
            // Attack-move
            if (!state->unitManager()->selected().isEmpty()) {
                state->unitManager()->selectAttackMoveTarget();
            }
        }
        return true;
    case input::Key::P: // Patrol
        if (!state->unitManager()->selected().isEmpty()) {
            state->unitManager()->selectPatrolTarget();
        }
        return true;
    case input::Key::G: // Guard
        if (!state->unitManager()->selected().isEmpty()) {
            state->unitManager()->selectGuardTarget();
        }
        return true;
    case input::Key::Delete: // Delete selected units
        for (const Unit::Ptr &unit : state->unitManager()->selected()) {
            unit->kill();
        }
        return true;
    case input::Key::Escape:
        if (m_chat.active) {
            m_chat.active = false;
            m_chat.buffer.clear();
#ifdef USE_SDL2
            SDL_StopTextInput();
#endif
            return true;
        }
        if (m_statsVisible) {
            m_statsVisible = false;
            return true;
        }
        showMenu();
        return true;
    case input::Key::Return:
        if (m_chat.active) {
            if (!m_chat.buffer.empty()) {
                const Player::Ptr &human = state->humanPlayer();
                int playerId = human ? human->playerId : 1;
                if (m_lockstep && m_lockstep->isMultiplayer()) {
                    // Route chat through lockstep so all players see it at the same game time
                    GameCommand chatCmd;
                    chatCmd.type = CommandType::Chat;
                    chatCmd.playerId = playerId;
                    chatCmd.targetId = m_chat.target;
                    chatCmd.message = m_chat.buffer;
                    m_lockstep->addCommand(chatCmd);
                } else {
                    EventManager::sendChatMessage(playerId, m_chat.target, m_chat.buffer);
                }
            }
            m_chat.buffer.clear();
            m_chat.active = false;
#ifdef USE_SDL2
            SDL_StopTextInput();
#endif
        } else {
            m_chat.active = true;
            m_chat.buffer.clear();
            m_chat.target = -1; // default to All
#ifdef USE_SDL2
            SDL_StartTextInput();
#endif
        }
        return true;
    case input::Key::Tab:
        if (m_chat.active) {
            // Cycle: All (-1) -> Allies (-2) -> All (-1)
            m_chat.target = (m_chat.target == -1) ? -2 : -1;
            return true;
        }
        // Toggle stats overlay
        m_statsVisible = !m_statsVisible;
        return true;
    case input::Key::BackSpace:
        if (m_chat.active && !m_chat.buffer.empty()) {
            m_chat.buffer.pop_back();
            return true;
        }
        break;

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
                    // Double-tap: center camera on group
                    int64_t now = currentTimeMs();
                    if (m_lastGroupKey == groupNum && now - m_lastGroupKeyTime < 500) {
                        // Average position of group
                        MapPos center;
                        for (const auto &u : m_controlGroups[groupNum]) {
                            center += u->position();
                        }
                        center /= m_controlGroups[groupNum].size();
                        renderTarget_->camera()->setTargetPosition(center);
                    }
                    m_lastGroupKey = groupNum;
                    m_lastGroupKeyTime = now;
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



    // F2 = hotkey help
    case input::Key::F2: {
        addMessage("--- Hotkey Reference ---");
        addMessage("H = Cycle Town Centers  |  F1 = Cycle Idle Villagers");
        addMessage("F2 = This Help  |  F3 = Pause  |  F4 = Minimap Mode");
        addMessage("F5 = Quick Save  |  F9 = Quick Load  |  F6 = Follow Unit");
        addMessage("Tab = Stats  |  F11 = Objectives  |  Esc = Menu");
        addMessage("S = Stop  |  A = Attack Move  |  Del = Delete Unit");
        addMessage("Ctrl+1..9 = Set Group  |  1..9 = Recall Group");
        addMessage("Enter = Chat  |  Shift+A = Select All Military");
        return true;
    }

    // H = cycle Town Centers
    case input::Key::H: {
        const Player::Ptr &human = state->humanPlayer();
        if (human) {
            // Collect all TCs (IDs: 109=TC, 71=TC foundation, 141=TC Age3, 142=TC Age4)
            std::vector<Unit::Ptr> tcs;
            for (const Unit::Ptr &unit : state->unitManager()->units()) {
                if (!unit || unit->isDead() || unit->isDying()) continue;
                if (unit->playerId() != human->playerId) continue;
                int id = unit->data()->ID;
                if (id == 109 || id == 71 || id == 141 || id == 142) {
                    tcs.push_back(unit);
                }
            }
            if (!tcs.empty()) {
                m_tcCycleIndex = m_tcCycleIndex % int(tcs.size());
                const Unit::Ptr &tc = tcs[m_tcCycleIndex];
                renderTarget_->camera()->setTargetPosition(tc->position());
                // Select the TC
                UnitVector sel;
                sel.push_back(tc);
                state->unitManager()->setSelectedUnits(sel);
                m_tcCycleIndex = (m_tcCycleIndex + 1) % int(tcs.size());
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

    // Edge panning is now handled in updateCamera() via SDL_GetMouseState

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
            if (!m_chat.active) {
                m_chat.active = true;
                m_chat.buffer.clear();
                m_chat.target = -1;
#ifdef USE_SDL2
                SDL_StartTextInput();
#endif
            }
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

        // Building/wall placement: if in PlacingBuilding state, tap places the building
        if (state->unitManager()->state() == UnitManager::State::PlacingBuilding ||
            state->unitManager()->state() == UnitManager::State::PlacingWall) {
            MapPos mapPos = renderTarget_->camera()->absoluteMapPos(pos).clamped(state->map()->pixelSize());
            state->unitManager()->onMouseMove(mapPos);
            state->unitManager()->onMouseRelease();
            m_touchState.phase = TouchState::Phase::Idle;
            m_touchState.tapTime = 0;
            return true;
        }

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
        Unit::Ptr unitAtTap = state->unitManager()->unitAt(pos, renderTarget_->camera(), NoAlignment);
        bool hasUnitAtTap = unitAtTap != nullptr;

        // If own units selected and tapping on a non-own unit (resource, enemy) →
        // treat as right-click (gather/attack) instead of selecting
        if (hasUnitAtTap && !state->unitManager()->selected().isEmpty()) {
            Player::Ptr humanPlayer = state->humanPlayer();
            bool tappedOwnUnit = humanPlayer && unitAtTap->playerId() == humanPlayer->playerId;
            if (!tappedOwnUnit) {
                // Check if we have own units selected (not just enemy units)
                bool hasOwnSelected = false;
                for (const Unit::Ptr &sel : state->unitManager()->selected()) {
                    if (humanPlayer && sel->playerId() == humanPlayer->playerId) {
                        hasOwnSelected = true;
                        break;
                    }
                }
                if (hasOwnSelected) {
                    // Tap on resource/enemy with own units selected = right-click (gather/attack)
                    state->unitManager()->onCursorPositionChanged(pos, renderTarget_->camera());
                    state->unitManager()->onRightClick(pos, renderTarget_->camera());
                    m_touchState.phase = TouchState::Phase::Idle;
                    m_touchState.tapTime = 0;
                    return true;
                }
            }
        }

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
        if (!m_chat.active) {
            m_chat.active = true;
            m_chat.buffer.clear();
            m_chat.target = -1;
#ifdef USE_SDL2
            SDL_StartTextInput();
#endif
        }
    } else if (clickedButton == IconButton::TechTree) {
        addMessage("Tech Tree: not yet implemented");
    } else if (clickedButton == IconButton::Settings) {
        addMessage("Settings: not yet implemented");
    }
    if (clickedButton != IconButton::Invalid) {
        return true;
    }

    // Minimap mode label click
    if (event.mouseButton.button == input::MouseButton::Left && m_minimap) {
        ScreenRect mmRect = m_minimap->rect();
        ScreenRect labelRect(mmRect.x, mmRect.y + mmRect.height, mmRect.width, 18);
        if (labelRect.contains(mousePos)) {
            m_minimap->cycleMode();
            return true;
        }
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

        // Ctrl+click: toggle a single unit in the selection
        const bool ctrlHeld = (SDL_GetModState() & KMOD_CTRL) != 0;
        if (ctrlHeld) {
            Unit::Ptr clicked = state->unitManager()->clickedUnitAt(mousePos, renderTarget_->camera());
            if (clicked && clicked->playerId() == state->unitManager()->humanPlayerID()) {
                state->unitManager()->toggleUnitInSelection(clicked);
                m_selectionRect = ScreenRect();
                m_selecting = false;
                return true;
            }
        }

        // Double-click: select all visible units of same type
        if (event.mouseButton.clicks >= 2) {
            Unit::Ptr clicked = state->unitManager()->clickedUnitAt(mousePos, renderTarget_->camera());
            if (clicked && clicked->playerId() == state->unitManager()->humanPlayerID()) {
                // Use the full viewport as the area
                const Size viewSize = renderTarget_->getSize();
                ScreenRect viewArea(0, 0, viewSize.width, viewSize.height);
                state->unitManager()->selectUnitsByType(clicked->data()->ID, clicked->playerId(), viewArea, renderTarget_->camera());
                m_selectionRect = ScreenRect();
                m_selecting = false;
                return true;
            }
        }

        state->unitManager()->selectUnits(selectRect, renderTarget_->camera());
        m_selectionRect = ScreenRect();
        m_selecting = false;
        return true;
    }
    if (event.mouseButton.button == input::MouseButton::Right) {
        // Ensure tasks under cursor are evaluated at click position
        state->unitManager()->onCursorPositionChanged(mousePos, renderTarget_->camera());
        const bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
        state->unitManager()->onRightClick(mousePos, renderTarget_->camera(), shiftHeld);
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
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1"); // handle back button in SDL
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");   // don't block when app paused
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

    m_helpText = renderTarget_->createText(Drawable::Text::Plain);
    m_helpText->pointSize = 13;
    m_helpText->color = Drawable::Color(255, 240, 180, 230);

    m_statText = renderTarget_->createText(Drawable::Text::Plain);
    m_statText->pointSize = 14;
    m_statText->color = Drawable::Color(200, 190, 150, 255);

    m_minimapModeText = renderTarget_->createText(Drawable::Text::Plain);
    m_minimapModeText->pointSize = 10;
    m_minimapModeText->color = Drawable::Color(200, 180, 130, 220);

    m_menuItemText = renderTarget_->createText(Drawable::Text::Plain);
    m_menuItemText->pointSize = 16;
    m_menuItemText->color = Drawable::Color(220, 200, 160, 255);

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

        // Extract briefing text from scenario for pre-game overlay
        std::string briefing = scenario->scenarioInstructions;
        if (briefing.empty()) {
            briefing = scenario->playerData.instructions;
        }
        // Strip null characters
        briefing.erase(std::remove(briefing.begin(), briefing.end(), '\0'), briefing.end());
        if (!briefing.empty()) {
            m_scenarioBriefing = briefing;
            m_showBriefing = true;
        }
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
    m_lobbyScreen = std::make_unique<LobbyScreen>(renderTarget_);
    m_settingsScreen = std::make_unique<SettingsScreen>(renderTarget_);
    m_settingsScreen->setEngineGameSpeed(&m_gameSpeed);
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

    EventManager::registerListener(this, EventManager::ChatMessage);

    m_btnReturnToMenu.text = "Return to Menu";
    m_btnContinuePlaying.text = "Continue Playing";
    m_btnReturnToMenu.setRenderTarget(renderTarget_);
    m_btnContinuePlaying.setRenderTarget(renderTarget_);

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
    updateAmbientSounds(state);

    updated = updateCamera(state) || updated;

    updated = m_minimap->update(deltaTime) || updated;
    updated = m_actionPanel->update(deltaTime) || updated;
    updated = m_unitInfoPanel->update(deltaTime) || updated;

    m_lastUpdate = Engine::currentTimeMs();
    return updated;
}

bool Engine::updateCamera(const std::shared_ptr<GameState> &state)
{
    // Camera follow: track the followed unit
    if (auto followUnit = m_followUnit.lock()) {
        if (followUnit->isDead() || followUnit->isDying()) {
            m_followUnit.reset();
        } else {
            renderTarget_->camera()->setTargetPosition(followUnit->position());
        }
    }

#ifdef ANDROID
    return false; // Camera controlled by touch drag
#endif

    // Query mouse position directly — pans even when mouse is stationary at edge
    {
        int mouseX = 0, mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        const Size windowSize = renderTarget_->getSize();
        constexpr int EDGE = 20;

        if (mouseX < EDGE) {
            m_cameraDeltaX = -1;
        } else if (mouseX > windowSize.width - EDGE) {
            m_cameraDeltaX = 1;
        } else {
            m_cameraDeltaX = 0;
        }

        if (mouseY < EDGE) {
            m_cameraDeltaY = 1;
        } else if (mouseY > windowSize.height - EDGE) {
            m_cameraDeltaY = -1;
        } else {
            m_cameraDeltaY = 0;
        }
    }

    if (m_cameraDeltaX == 0 && m_cameraDeltaY == 0) {
        return false;
    }

    // Manual camera movement cancels follow
    m_followUnit.reset();

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

void Engine::loadNextCampaignScenario()
{
    if (m_campaignPath.empty() || m_campaignScenarioIndex < 0) return;

    const int nextIndex = m_campaignScenarioIndex + 1;
    if (nextIndex >= m_campaignScenarioCount) {
        addMessage("Campaign Complete!");
        if (m_resultOverlay) {
            m_resultOverlay->string = "Campaign Complete!";
        }
        return;
    }

    try {
        genie::CpxFile cpx;
        cpx.load(m_campaignPath);
        genie::ScnFilePtr nextScenario = cpx.getScnFile(nextIndex);
        if (!nextScenario) {
            WARN << "Failed to load scenario" << nextIndex << "from" << m_campaignPath;
            return;
        }
        addMessage("Loading next mission...");
        m_campaignScenarioIndex = nextIndex;
        setup(nextScenario);
    } catch (const std::exception &ex) {
        WARN << "Exception loading next campaign scenario:" << ex.what();
    }
}

void Engine::onChatMessage(const int sourcePlayer, const int /*targetPlayer*/, const std::string &message)
{
    // Handle cheat codes
    if (message == "marco") {
        auto state = state_manager_.getActiveState();
        if (state) {
            const Player::Ptr &human = state->humanPlayer();
            if (human && human->visibility) {
                human->visibility->revealAll();
                addMessage("Map revealed!");
                return;
            }
        }
    }

    // Chat taunts (1-42): numeric message maps to taunt text
    static const char *s_taunts[42] = {
        /*  1 */ "Yes",
        /*  2 */ "No",
        /*  3 */ "Food please",
        /*  4 */ "Wood please",
        /*  5 */ "Gold please",
        /*  6 */ "Stone please",
        /*  7 */ "Ahh!",
        /*  8 */ "All hail, king of the losers!",
        /*  9 */ "Oooh!",
        /* 10 */ "I'll beat you back to Age of Empires",
        /* 11 */ "Hahahah!",
        /* 12 */ "Ack! He rushed!",
        /* 13 */ "Sure, blame it on your ISP",
        /* 14 */ "Start the game already!",
        /* 15 */ "Don't point that thing at me!",
        /* 16 */ "Enemy sighted!",
        /* 17 */ "It is good to be the king",
        /* 18 */ "Monk! I need a monk!",
        /* 19 */ "Long time, no siege",
        /* 20 */ "My granny could scrap better than that",
        /* 21 */ "Nice town, I'll take it",
        /* 22 */ "Quit touching me!",
        /* 23 */ "Raiding party!",
        /* 24 */ "Dadgum",
        /* 25 */ "Eh, smite me",
        /* 26 */ "The wonder, the wonder, the... no!",
        /* 27 */ "You played two hours to die like this?",
        /* 28 */ "Yeah, well, you should see the other guy",
        /* 29 */ "Rogan?",
        /* 30 */ "Wololo",
        /* 31 */ "Attack an enemy now!",
        /* 32 */ "Cease creating extra villagers",
        /* 33 */ "Create extra villagers",
        /* 34 */ "Build a navy",
        /* 35 */ "Stop building a navy",
        /* 36 */ "Wait for my signal to attack",
        /* 37 */ "Build a wonder",
        /* 38 */ "Give me your extra resources",
        /* 39 */ "(Ally sound)",
        /* 40 */ "(Enemy sound)",
        /* 41 */ "(Neutral sound)",
        /* 42 */ "What age are you in?"
    };

    // Check if message is a number 1-42
    bool isNumeric = !message.empty();
    for (char c : message) {
        if (c < '0' || c > '9') { isNumeric = false; break; }
    }
    if (isNumeric) {
        int num = std::atoi(message.c_str());
        if (num >= 1 && num <= 42) {
            std::string tauntText = s_taunts[num - 1];
            std::string display = "Player " + std::to_string(sourcePlayer) + ": " + tauntText;
            addMessage(display);

            // Try to play taunt audio (tauntNN.mp3)
            char tauntFile[32];
            snprintf(tauntFile, sizeof(tauntFile), "taunt%02d.mp3", num);
            AudioPlayer::instance().playStream(std::string(tauntFile));
            return;
        }
    }

    std::string display = "Player " + std::to_string(sourcePlayer) + ": " + message;
    addMessage(display);
}

void Engine::updateAmbientSounds(const std::shared_ptr<GameState> &state)
{
    if (!m_mapRenderer || !state || !state->map()) return;

    // Only update every 500ms to avoid thrashing
    int64_t now = currentTimeMs();
    if (now - m_lastAmbientUpdate < 500) return;
    m_lastAmbientUpdate = now;

    int waterTiles = 0;
    int treeTiles = 0;
    int totalTiles = 0;

    const int colBegin = m_mapRenderer->firstVisibleColumn();
    const int colEnd = m_mapRenderer->lastVisibleColumn();
    const int rowBegin = m_mapRenderer->firstVisibleRow();
    const int rowEnd = m_mapRenderer->lastVisibleRow();

    const MapPtr &map = state->map();

    for (int col = colBegin; col < colEnd; col++) {
        for (int row = rowBegin; row < rowEnd; row++) {
            totalTiles++;
            const MapTile &tile = map->getTileAt(col, row);
            int id = tile.terrainId;
            // Water terrain IDs: 1=shallow, 2=medium, 3=deep, 4=ocean, 22=deep, 26=beach
            if (id == 1 || id == 2 || id == 3 || id == 4 || id == 22 || id == 26) {
                waterTiles++;
            }
            // Forest terrain IDs: 10=forest, 13=palm, 17=jungle, 18=bamboo, 19=pine
            if (id == 10 || id == 13 || id == 17 || id == 18 || id == 19) {
                treeTiles++;
            }
        }
    }

    if (totalTiles == 0) return;

    // Water ambient: sound 326 (water lapping)
    float waterRatio = static_cast<float>(waterTiles) / totalTiles;
    if (waterRatio > 0.05f) {
        float vol = std::clamp(waterRatio * 0.5f, 0.05f, 0.3f);
        AudioPlayer::instance().startAmbientLoop(0, 326, 0);
        AudioPlayer::instance().setAmbientVolume(0, vol);
    } else {
        AudioPlayer::instance().stopAmbientLoop(0);
    }

    // Forest ambient: sound 330 (birds/forest)
    float treeRatio = static_cast<float>(treeTiles) / totalTiles;
    if (treeRatio > 0.05f) {
        float vol = std::clamp(treeRatio * 0.4f, 0.05f, 0.25f);
        AudioPlayer::instance().startAmbientLoop(1, 330, 0);
        AudioPlayer::instance().setAmbientVolume(1, vol);
    } else {
        AudioPlayer::instance().stopAmbientLoop(1);
    }
}

//------------------------------------------------------------------------------
// Save/Load UI
//------------------------------------------------------------------------------

static std::string savesDirectory()
{
#ifdef __ANDROID__
    const char *ext = SDL_AndroidGetExternalStoragePath();
    return ext ? std::string(ext) : "/sdcard";
#elif defined(USE_SDL2)
    char *prefPath = SDL_GetPrefPath("freeaoe", "freeaoe");
    std::string dir;
    if (prefPath) {
        dir = std::string(prefPath);
        SDL_free(prefPath);
    } else {
        dir = ".";
    }
    return dir;
#else
    return ".";
#endif
}

void Engine::showSaveLoadScreen(bool loadMode)
{
    m_saveLoadScreen.visible = true;
    m_saveLoadScreen.loadMode = loadMode;
    m_saveLoadScreen.selectedIndex = -1;
    m_saveLoadScreen.newSaveName.clear();
    m_saveLoadScreen.files = SaveGame::listSaves(savesDirectory());
}

void Engine::renderSaveLoadScreen()
{
    if (!m_saveLoadScreen.visible) return;

    Size screenSize = renderTarget_->getSize();

    // Fullscreen dim
    renderTarget_->draw(ScreenRect(0, 0, screenSize.width, screenSize.height),
        Drawable::Color(0, 0, 0, 180));

    // Panel
    float panelW = std::min(520.f, screenSize.width - 40.f);
    float panelH = std::min(450.f, screenSize.height - 60.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    renderTarget_->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Color(35, 25, 12, 245));
    renderTarget_->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

    // Title
    auto titleText = renderTarget_->createText(Drawable::Text::UI);
    titleText->pointSize = 22;
    titleText->color = Drawable::Color(255, 220, 150, 255);
    titleText->string = m_saveLoadScreen.loadMode ? "Load Game" : "Save Game";
    titleText->position = ScreenPos(panelX + 20, panelY + 12);
    renderTarget_->draw(titleText);

    // Reuse m_statText for labels (same pattern as other overlays)
    auto &labelText = m_statText;

    // File list area
    float listX = panelX + 15;
    float listY = panelY + 50;
    float listW = panelW - 30;
    float itemH = 32;
    int maxVisible = static_cast<int>((panelH - 140) / itemH);

    for (int i = 0; i < static_cast<int>(m_saveLoadScreen.files.size()) && i < maxVisible; i++) {
        float iy = listY + i * itemH;
        bool selected = (i == m_saveLoadScreen.selectedIndex);

        // Background
        Drawable::Color bg = selected
            ? Drawable::Color(80, 60, 30, 220)
            : Drawable::Color(20, 15, 8, 180);
        renderTarget_->draw(ScreenRect(listX, iy, listW, itemH - 2), bg);

        // Extract filename from full path
        std::string displayName = m_saveLoadScreen.files[i];
        size_t lastSlash = displayName.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            displayName = displayName.substr(lastSlash + 1);
        }

        labelText->string = displayName;
        labelText->color = selected
            ? Drawable::Color(255, 230, 160, 255)
            : Drawable::Color(200, 180, 140, 255);
        labelText->position = ScreenPos(listX + 8, iy + 6);
        renderTarget_->draw(labelText);
    }

    if (m_saveLoadScreen.files.empty()) {
        labelText->string = "(no save files found)";
        labelText->color = Drawable::Color(150, 130, 100, 200);
        labelText->position = ScreenPos(listX + 8, listY + 6);
        renderTarget_->draw(labelText);
    }

    // Buttons at the bottom
    float btnY = panelY + panelH - 50;
    float btnW = 100;
    float btnH = 36;

    // OK button
    float okX = panelX + panelW / 2 - btnW - 10;
    renderTarget_->draw(ScreenRect(okX, btnY, btnW, btnH),
        Drawable::Color(60, 45, 20, 230));
    renderTarget_->draw(ScreenRect(okX, btnY, btnW, btnH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));
    labelText->string = m_saveLoadScreen.loadMode ? "Load" : "Save";
    labelText->color = Drawable::Color(220, 200, 160, 255);
    labelText->position = ScreenPos(okX + 30, btnY + 8);
    renderTarget_->draw(labelText);

    // Cancel button
    float cancelX = panelX + panelW / 2 + 10;
    renderTarget_->draw(ScreenRect(cancelX, btnY, btnW, btnH),
        Drawable::Color(60, 45, 20, 230));
    renderTarget_->draw(ScreenRect(cancelX, btnY, btnW, btnH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));
    labelText->string = "Cancel";
    labelText->color = Drawable::Color(220, 200, 160, 255);
    labelText->position = ScreenPos(cancelX + 20, btnY + 8);
    renderTarget_->draw(labelText);
}

bool Engine::handleSaveLoadEvent(const input::Event &event)
{
    if (!m_saveLoadScreen.visible) return false;

    ScreenPos pos;
    bool isTap = false;

    if (event.type == input::Event::TouchEnded || event.type == input::Event::MouseButtonReleased) {
        pos = (event.type == input::Event::TouchEnded)
            ? ScreenPos(event.touch.x, event.touch.y)
            : ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isTap = true;
    } else if (event.type == input::Event::KeyPressed && event.key.code == input::Key::Escape) {
        m_saveLoadScreen.visible = false;
        return true;
    }

    if (!isTap) return true; // consume all events while visible

    Size screenSize = renderTarget_->getSize();
    float panelW = std::min(520.f, screenSize.width - 40.f);
    float panelH = std::min(450.f, screenSize.height - 60.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    // File list hit test
    float listX = panelX + 15;
    float listY = panelY + 50;
    float listW = panelW - 30;
    float itemH = 32;
    int maxVisible = static_cast<int>((panelH - 140) / itemH);

    for (int i = 0; i < static_cast<int>(m_saveLoadScreen.files.size()) && i < maxVisible; i++) {
        float iy = listY + i * itemH;
        if (ScreenRect(listX, iy, listW, itemH).contains(pos)) {
            m_saveLoadScreen.selectedIndex = i;
            return true;
        }
    }

    // Button hit test
    float btnY = panelY + panelH - 50;
    float btnW = 100;
    float btnH = 36;
    float okX = panelX + panelW / 2 - btnW - 10;
    float cancelX = panelX + panelW / 2 + 10;

    // OK button
    if (ScreenRect(okX, btnY, btnW, btnH).contains(pos)) {
        auto state = state_manager_.getActiveState();
        if (m_saveLoadScreen.loadMode) {
            // Load selected file
            if (m_saveLoadScreen.selectedIndex >= 0 &&
                m_saveLoadScreen.selectedIndex < static_cast<int>(m_saveLoadScreen.files.size())) {
                const std::string &path = m_saveLoadScreen.files[m_saveLoadScreen.selectedIndex];
                float camX = 0, camY = 0;
                if (SaveGame::load(path, *state, camX, camY)) {
                    renderTarget_->camera()->setTargetPosition(MapPos(camX, camY));
                    addMessage("Game loaded!");
                } else {
                    addMessage("Failed to load game!");
                }
            }
        } else {
            // Save to new file with timestamp
            std::string dir = savesDirectory();
            time_t now = time(nullptr);
            struct tm *t = localtime(&now);
            char nameBuf[64];
            snprintf(nameBuf, sizeof(nameBuf), "save_%04d%02d%02d_%02d%02d%02d.faoe",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
            std::string savePath = dir + "/" + nameBuf;
            MapPos camPos = renderTarget_->camera()->targetPosition();
            if (SaveGame::save(savePath, *state, camPos.x, camPos.y)) {
                addMessage("Game saved: " + std::string(nameBuf));
            } else {
                addMessage("Save failed!");
            }
        }
        m_saveLoadScreen.visible = false;
        return true;
    }

    // Cancel button
    if (ScreenRect(cancelX, btnY, btnW, btnH).contains(pos)) {
        m_saveLoadScreen.visible = false;
        return true;
    }

    // Click outside panel = close
    if (!ScreenRect(panelX, panelY, panelW, panelH).contains(pos)) {
        m_saveLoadScreen.visible = false;
        return true;
    }

    return true;
}
