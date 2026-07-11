#/*
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


#include "Editor.h"

#include <algorithm>
#include <utility>

#include <stddef.h>
//------------------------------------------------------------------------------
Editor::Editor() :
    UiScreen("scr4.sin")
{
}

// Just to make the crappy gcc unique_ptr implementation work
Editor::~Editor() { }

void Editor::render()
{
    m_exitButton.render(this);
}

bool Editor::init()
{
    if (!UiScreen::init()) {
        return false;
    }

    m_exitButton.text = "Not implemented, click to exit";
    m_exitButton.rect.width = 300;
    m_exitButton.rect.height = 50;
    m_exitButton.rect.x = m_backgroundSize.width / 2.f - m_exitButton.rect.width / 2.f;
    m_exitButton.rect.y = m_backgroundSize.height / 2.f - m_exitButton.rect.height / 2.f;

    return true;
}

bool Editor::handleMouseEvent(const input::Event &event)
{
    if (event.type == input::Event::MouseButtonPressed) {
        ScreenPos mousePos(event.mouseButton.x, event.mouseButton.y);
        if (m_exitButton.rect.contains(mousePos)) {
            m_exitButton.pressed = true;
        }
    }

    if (event.type == input::Event::MouseButtonReleased) {
        if (m_exitButton.pressed) {
            return true;
        }
    }

    return false;
}

