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
#pragma once

#include <memory>
#include <string>

#include "core/Types.h"
#include "render/IRenderTarget.h"
#include "render/EventTypes.h"

namespace sf {
class RenderWindow;
}

namespace genie {
class UIFile;
class SlpFile;
}

class UiScreen
{
public:
    UiScreen(const char *uiFile);
    UiScreen() = delete;
    virtual ~UiScreen() {}

    virtual bool init();
    bool run();

    virtual void render() {}
    virtual bool handleMouseEvent(const input::Event &event) { (void)event; return false; }
    virtual void handleKeyEvent(const input::Event &) {}

    void setRenderWindow(const std::shared_ptr<sf::RenderWindow> &renderWindow);

protected:
    friend struct TextButton;


    Size m_backgroundSize;
    Drawable::Color m_textFillColor;
    Drawable::Color m_textOutlineColor;

    Drawable::Color m_bevelColor1a;
    Drawable::Color m_bevelColor1b;
    Drawable::Color m_bevelColor1c;

    Drawable::Color m_bevelColor2a;
    Drawable::Color m_bevelColor2b;
    Drawable::Color m_bevelColor2c;

    int m_pressOffset = 0;

    int m_paletteId = 0;
    float m_buttonOpacity = 1.f;

    std::string m_uiFileName;
    std::shared_ptr<genie::UIFile> m_uiFile;
    std::shared_ptr<genie::SlpFile> m_backgroundSlp;
    std::shared_ptr<sf::RenderWindow> m_renderWindow;
    std::shared_ptr<IRenderTarget> m_renderTarget;
    Drawable::Image::Ptr m_background;
};

