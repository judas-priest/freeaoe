/*
    Copyright (C) 2019 Martin Sandsmark <martin.sandsmark@kde.org>

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

#ifndef USE_SDL2
#ifndef USE_SDL2
#include <SFML/Graphics/Text.hpp>
#endif
#endif
#include <string>

#include "core/Types.h"
#include "render/IRenderTarget.h"

class UiScreen;

struct TextButton
{
    std::string text;
    ScreenRect rect;
    bool pressed = false;

    TextButton();

    void render(UiScreen *screen);

#ifdef USE_SDL2
    void setRenderTarget(const std::shared_ptr<IRenderTarget> &rt);
private:
    std::shared_ptr<IRenderTarget> m_renderTarget;
    Drawable::Text::Ptr m_text;
#else
private:
    static void drawLine(const ScreenPos &from, const ScreenPos &to, const sf::Color &color, UiScreen *screen);
    sf::Text m_text;
#endif
};
