#pragma once

#include "TextButton.h"
#include "render/IRenderTarget.h"

#include <array>
#include <memory>

struct TextButton;

namespace sf {
class Event;
class RenderWindow;
}

class UiScreen;

struct Dialog
{
    enum Choice {
        Invalid = -1,
        Quit,
        Achievements,
        Save,
        Options,
        About,
        Cancel,
        ChoicesCount
    };

    Dialog(UiScreen *screen);

    void render(std::shared_ptr<sf::RenderWindow> &renderTarget);
    Choice handleEvent(const sf::Event &event);

    Drawable::Image::Ptr background;
    UiScreen *m_screen;

private:
    std::array<TextButton, ChoicesCount> m_buttons;
    Choice m_pressedButton = Invalid;
};

