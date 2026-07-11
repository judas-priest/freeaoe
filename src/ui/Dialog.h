#pragma once

#include "TextButton.h"
#include "render/IRenderTarget.h"
#include "render/EventTypes.h"

#include <array>
#include <memory>

struct TextButton;

#ifndef USE_SDL2
namespace sf {
class RenderWindow;
}
#endif

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

#ifdef USE_SDL2
    void render(const std::shared_ptr<IRenderTarget> &renderTarget);
#else
    void render(std::shared_ptr<sf::RenderWindow> &renderTarget);
#endif
    Choice handleEvent(const input::Event &event);

    Drawable::Image::Ptr background;
    UiScreen *m_screen;

private:
    std::array<TextButton, ChoicesCount> m_buttons;
    Choice m_pressedButton = Invalid;
};

