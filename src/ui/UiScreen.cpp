/*
    Copyright (C) 2018 Martin Sandsmark <martin.sandsmark@kde.org>

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
#include "UiScreen.h"

#include "core/Logger.h"
#include "core/Types.h"
#ifdef USE_SDL2
#include "render/SdlRenderTarget.h"
#else
#include "render/SfmlRenderTarget.h"
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#endif
#include "resource/AssetManager.h"
#include "resource/DataManager.h"
#include "resource/Resource.h"

#include <genie/resource/Color.h>
#include <genie/resource/PalFile.h>
#include <genie/resource/SlpFile.h>
#include <genie/resource/SlpFrame.h>
#include <genie/resource/UIFile.h>

#include <vector>

#ifndef USE_SDL2
static input::Key sfKeyToInputKey(sf::Keyboard::Key key) {
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

static input::MouseButton sfMouseBtnToInput(sf::Mouse::Button button) {
    switch(button) {
    case sf::Mouse::Left: return input::MouseButton::Left;
    case sf::Mouse::Right: return input::MouseButton::Right;
    case sf::Mouse::Middle: return input::MouseButton::Middle;
    default: return input::MouseButton::Left;
    }
}

static input::Event convertSfEvent(const sf::Event &sfEvent) {
    input::Event ev{};
    switch(sfEvent.type) {
    case sf::Event::Closed: ev.type = input::Event::Closed; break;
    case sf::Event::KeyPressed:
        ev.type = input::Event::KeyPressed;
        ev.key.code = sfKeyToInputKey(sfEvent.key.code);
        ev.key.shift = sfEvent.key.shift;
        ev.key.control = sfEvent.key.control;
        ev.key.alt = sfEvent.key.alt;
        break;
    case sf::Event::KeyReleased:
        ev.type = input::Event::KeyReleased;
        ev.key.code = sfKeyToInputKey(sfEvent.key.code);
        break;
    case sf::Event::MouseButtonPressed:
        ev.type = input::Event::MouseButtonPressed;
        ev.mouseButton.button = sfMouseBtnToInput(sfEvent.mouseButton.button);
        ev.mouseButton.x = sfEvent.mouseButton.x;
        ev.mouseButton.y = sfEvent.mouseButton.y;
        break;
    case sf::Event::MouseButtonReleased:
        ev.type = input::Event::MouseButtonReleased;
        ev.mouseButton.button = sfMouseBtnToInput(sfEvent.mouseButton.button);
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
    default: ev.type = input::Event::Closed; break;
    }
    return ev;
}
#endif // !USE_SDL2

UiScreen::UiScreen(const char *uiFile) :
    m_uiFileName(uiFile)
{

}

static Drawable::Color convertColor(const genie::Color &color)
{
    return Drawable::Color(color.r, color.g, color.b);
}

bool UiScreen::init()
{
    if (!DataManager::Inst().isHd() || DataManager::Inst().gameVersion() >= genie::GV_SWGB) {
        const std::string uiFilename = 'x' + m_uiFileName;

        m_uiFile = AssetManager::Inst()->getUIFile(uiFilename);

        if (m_uiFile) {
            m_uiFileName = uiFilename;
        } else {
            DBG << "Failed to find" << m_uiFileName << "with x prefix, trying without";
        }
    }

    if (!m_uiFile) {
        m_uiFile = AssetManager::Inst()->getUIFile(m_uiFileName);
    }

    if (!m_uiFile) {
        WARN << "Unable to load ui file" << m_uiFileName;
        return false;
    }

    m_buttonOpacity = m_uiFile->shadePercent / 100.;

    m_textFillColor = Drawable::Color(m_uiFile->textColor1.r, m_uiFile->textColor1.g, m_uiFile->textColor1.b);
    m_textOutlineColor = Drawable::Color(m_uiFile->textColor2.r, m_uiFile->textColor2.g, m_uiFile->textColor2.b);

//    m_bevelColor1 = sf::Color(m_uiFile->bevelColor1.r, m_uiFile->bevelColor1.g, m_uiFile->bevelColor1.b);
//    m_bevelColor2 = sf::Color(m_uiFile->bevelColor2.r, m_uiFile->bevelColor2.g, m_uiFile->bevelColor2.b);

    m_paletteId = m_uiFile->paletteFile.id;
    const genie::PalFile &palette = AssetManager::Inst()->getPalette(m_paletteId);
    const std::vector<genie::Color> &colors = palette.getColors();

    m_bevelColor1a = convertColor(colors[m_uiFile->bevelColor1.r]);
    m_bevelColor1b = convertColor(colors[m_uiFile->bevelColor1.g]);
    m_bevelColor1c = convertColor(colors[m_uiFile->bevelColor1.b]);

    m_bevelColor2a = convertColor(colors[m_uiFile->bevelColor2.r]);
    m_bevelColor2b = convertColor(colors[m_uiFile->bevelColor2.g]);
    m_bevelColor2c = convertColor(colors[m_uiFile->bevelColor2.b]);

    m_pressOffset = m_uiFile->backgroundPosition;

#ifdef USE_SDL2
    if (!m_renderTarget) {
        m_backgroundSlp = AssetManager::Inst()->getSlp(m_uiFile->backgroundLarge.fileId, AssetManager::ResourceType::Interface);
        if (!m_backgroundSlp) {
            DBG << "failed to load slp file for UI screen by ID, trying name";

            std::string backgroundName;
            if (m_uiFile->backgroundLarge.filename != "none") {
                backgroundName = m_uiFile->backgroundLarge.filename;
            } else {
                backgroundName = m_uiFile->backgroundSmall.filename;
            }
            m_backgroundSlp = AssetManager::Inst()->getSlp(backgroundName + ".slp", AssetManager::ResourceType::Interface);
        }
        if (!m_backgroundSlp) {
            WARN << "failed to load slp file for UI screen";
            return false;
        }

        genie::SlpFramePtr backgroundFrame = m_backgroundSlp->getFrame(0);
        if (!backgroundFrame) {
            WARN << "Failed to get frame";
            return false;
        }

        const int width = backgroundFrame->getWidth();
        const int height = backgroundFrame->getHeight();

        m_backgroundSize = Size(width, height);

        auto sdlWindow = std::make_unique<SdlWindow>(Size(width, height), "freeaoe");
        m_renderTarget = std::make_shared<SdlRenderTarget>(Size(width, height), sdlWindow->sdlRenderer);

        DBG << backgroundFrame->getWidth() << backgroundFrame->getHeight();
        Resource::RawImage raw = Resource::convertFrameToImage(backgroundFrame, palette);
        m_background = m_renderTarget->createImage(Size(raw.width, raw.height), raw.pixels.data());
    }
#else
    if (!m_renderWindow) {
        m_backgroundSlp = AssetManager::Inst()->getSlp(m_uiFile->backgroundLarge.fileId, AssetManager::ResourceType::Interface);
        if (!m_backgroundSlp) {
            DBG << "failed to load slp file for UI screen by ID, trying name";

            std::string backgroundName;
            if (m_uiFile->backgroundLarge.filename != "none") {
                backgroundName = m_uiFile->backgroundLarge.filename;
            } else {
                backgroundName = m_uiFile->backgroundSmall.filename;
            }
            m_backgroundSlp = AssetManager::Inst()->getSlp(backgroundName + ".slp", AssetManager::ResourceType::Interface);
        }
        if (!m_backgroundSlp) {
            WARN << "failed to load slp file for UI screen";
            return false;
        }


        genie::SlpFramePtr backgroundFrame = m_backgroundSlp->getFrame(0);
        if (!backgroundFrame) {
            WARN << "Failed to get frame";
            return false;
        }

        const int width = backgroundFrame->getWidth();
        const int height = backgroundFrame->getHeight();

        m_backgroundSize = Size(width, height);

        m_renderWindow = std::make_shared<sf::RenderWindow>(sf::VideoMode(width, height), "freeaoe");
        m_renderWindow->setSize(sf::Vector2u(width, height));
        m_renderWindow->setView(sf::View(sf::FloatRect(0, 0, width, height)));

        m_renderTarget = std::make_shared<SfmlRenderTarget>(*m_renderWindow);

        DBG << backgroundFrame->getWidth() << backgroundFrame->getHeight();
        Resource::RawImage raw = Resource::convertFrameToImage(backgroundFrame, palette);
        m_background = m_renderTarget->createImage(Size(raw.width, raw.height), raw.pixels.data());
    }
#endif


    return true;
}

#ifndef USE_SDL2
void UiScreen::setRenderWindow(const std::shared_ptr<sf::RenderWindow> &renderWindow)
{
    m_renderWindow = renderWindow;
    if (renderWindow) {
        m_renderTarget = std::make_shared<SfmlRenderTarget>(*renderWindow);
    }
}
#endif

bool UiScreen::run()
{
#ifdef USE_SDL2
    // SDL2 run loop — UiScreen doesn't own its own window in SDL mode,
    // so this is a simplified polling loop. The SdlWindow used during init()
    // may have been temporary; for now, provide a stub that works with
    // the render target already set up.
    // TODO: full SDL2 UiScreen loop with SdlWindow ownership
    WARN << "UiScreen::run() not fully implemented for SDL2 backend";
    return true;
#else
    while (m_renderWindow->isOpen()) {
        // Process events
        sf::Event sfEvent;
        if (!m_renderWindow->waitEvent(sfEvent)) {
            WARN << "failed to get event";
            break;
        }

        if (sfEvent.type == sf::Event::Closed) {
            return false;
        }

        input::Event event = convertSfEvent(sfEvent);

        if (event.type == input::Event::MouseButtonPressed || event.type == input::Event::MouseButtonReleased) {
            sf::Vector2f mappedPos = m_renderWindow->mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
            event.mouseButton.x = mappedPos.x;
            event.mouseButton.y = mappedPos.y;
        }

        if (event.type == input::Event::MouseMoved) {
            sf::Vector2f mappedPos = m_renderWindow->mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y));
            event.mouseMove.x = mappedPos.x;
            event.mouseMove.y = mappedPos.y;
        }

        if (event.type == input::Event::KeyPressed) {
            handleKeyEvent(event);
        }

        if (handleMouseEvent(event)) {
            m_renderWindow->close();
            continue;
        }

        m_renderWindow->clear(sf::Color::Black);
        if (m_renderTarget && m_background) {
            m_renderTarget->draw(m_background, ScreenPos(0, 0));
        }
        render();
        m_renderWindow->display();
    }

    return true;
#endif
}
