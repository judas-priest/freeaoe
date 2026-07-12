#include "Dialog.h"

#ifndef USE_SDL2
#ifndef USE_SDL2
#include <SFML/Graphics/RenderWindow.hpp>
#endif
#include "render/SfmlRenderTarget.h"
#endif
#include <string>

#include "core/Types.h"

Dialog::Dialog(UiScreen *screen) :
    m_screen(screen)
{
    m_buttons[Quit].text = "Quit";
    m_buttons[Achievements].text = "Achivements (TODO)";
    m_buttons[Save].text = "Save (TODO)";
    m_buttons[Options].text = "Options (TODO)";
    m_buttons[About].text = "About (TODO)";
    m_buttons[Cancel].text = "Cancel";

}

#ifdef USE_SDL2
void Dialog::render(const std::shared_ptr<IRenderTarget> &renderTarget)
{
    Size windowSize = renderTarget->getSize();
    Size textureSize(295, 300);
    const ScreenPos windowCenter(windowSize.width / 2, windowSize.height / 2);
    ScreenPos position(windowCenter.x - textureSize.width/2, windowCenter.y - textureSize.height/2);

    // Semi-transparent overlay behind dialog
    renderTarget->draw(ScreenRect(0, 0, windowSize.width, windowSize.height),
                       Drawable::Color(0, 0, 0, 128));

    if (background) {
        renderTarget->draw(background, position);
    }
#else
void Dialog::render(std::shared_ptr<sf::RenderWindow> &renderTarget)
{
    Size windowSize = renderTarget->getSize();
    Size textureSize(295, 300); //background.getSize(); can't use the actual size, because of the shadow...
    const ScreenPos windowCenter(windowSize.width / 2, windowSize.height / 2);
    ScreenPos position(windowCenter.x - textureSize.width/2, windowCenter.y - textureSize.height/2);
    if (background) {
        SfmlRenderTarget sfmlRT(*renderTarget);
        sfmlRT.draw(background, position);
    }
#endif

    const int buttonWidth = textureSize.width - 80;
    const int buttonHeight = 30;
    const int buttonMargin = 10;

    const int allButtonsHeight = ChoicesCount * (buttonHeight + buttonMargin);


    const int x = windowCenter.x - buttonWidth / 2.f;
    int y = windowCenter.y - allButtonsHeight / 2.f;

    for (int i=0; i<ChoicesCount; i++) {
#ifdef USE_SDL2
        m_buttons[i].setRenderTarget(renderTarget);
#endif
        m_buttons[i].rect.x = x;
        m_buttons[i].rect.y = y;

        m_buttons[i].rect.height = buttonHeight;
        m_buttons[i].rect.width = buttonWidth;
        y += buttonHeight + buttonMargin;

        m_buttons[i].render(m_screen);
    }
}

Dialog::Choice Dialog::handleEvent(const input::Event &event)
{
    ScreenPos pos;
    bool isPress = false;

    if (event.type == input::Event::MouseButtonPressed) {
        pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isPress = true;
    } else if (event.type == input::Event::MouseButtonReleased) {
        pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
    } else if (event.type == input::Event::TouchBegan) {
        pos = ScreenPos(event.touch.x, event.touch.y);
        isPress = true;
    } else if (event.type == input::Event::TouchEnded) {
        pos = ScreenPos(event.touch.x, event.touch.y);
    } else {
        return Invalid;
    }

    if (isPress) {
        for (int i=0; i<ChoicesCount; i++) {
            m_buttons[i].pressed = m_buttons[i].rect.contains(pos);
            if (m_buttons[i].pressed) {
                m_pressedButton = Choice(i);
            }
        }
        return Invalid;
    }

    // Release
    Choice choice = Invalid;
    for (int i=0; i<ChoicesCount; i++) {
        m_buttons[i].pressed = false;
        if (m_buttons[i].rect.contains(pos)) {
            choice = Choice(i);
        }
    }
    if (choice != m_pressedButton) {
        m_pressedButton = Invalid;
    }
    return m_pressedButton;
}
