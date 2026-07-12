#include "TextButton.h"

#ifndef USE_SDL2
#ifndef USE_SDL2
#include <SFML/Graphics/Color.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Graphics/Rect.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Graphics/RectangleShape.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Graphics/RenderWindow.hpp>
#endif
#include "render/SfmlRenderTarget.h"
#endif
#include <memory>
#include <utility>

#include "UiScreen.h"

#ifdef USE_SDL2

#include "render/IRenderTarget.h"

TextButton::TextButton()
{
}

void TextButton::setRenderTarget(const std::shared_ptr<IRenderTarget> &rt)
{
    if (m_renderTarget == rt) {
        return;
    }
    m_renderTarget = rt;
    m_text = rt->createText(Drawable::Text::Plain);
    m_text->pointSize = 17;
}

void TextButton::render(UiScreen *screen)
{
    if (!m_renderTarget || !m_text) {
        return;
    }

    // Background
    Drawable::Color bgColor(0, 0, 0, static_cast<uint8_t>(screen->m_buttonOpacity * 255));
    m_renderTarget->draw(rect, bgColor);

    // Bevel colors — swap when pressed (same logic as SFML version)
    Drawable::Color inner1 = screen->m_bevelColor1a;
    Drawable::Color middle1 = screen->m_bevelColor1b;
    Drawable::Color outer1 = screen->m_bevelColor1c;
    Drawable::Color inner2 = screen->m_bevelColor2a;
    Drawable::Color middle2 = screen->m_bevelColor2b;
    Drawable::Color outer2 = screen->m_bevelColor2c;
    if (pressed) {
        std::swap(inner1, inner2);
        std::swap(middle1, middle2);
        std::swap(outer1, outer2);
    }

    // Top border (3 lines)
    m_renderTarget->draw(ScreenRect(rect.x - 2, rect.y - 2, rect.width + 4, 1), inner1);
    m_renderTarget->draw(ScreenRect(rect.x - 1, rect.y - 1, rect.width + 2, 1), middle1);
    m_renderTarget->draw(ScreenRect(rect.x,     rect.y,     rect.width,     1), outer1);

    // Bottom border
    m_renderTarget->draw(ScreenRect(rect.x - 2, rect.y + rect.height + 1, rect.width + 4, 1), inner2);
    m_renderTarget->draw(ScreenRect(rect.x - 1, rect.y + rect.height,     rect.width + 2, 1), middle2);
    m_renderTarget->draw(ScreenRect(rect.x,     rect.y + rect.height - 1, rect.width,     1), outer2);

    // Left border
    m_renderTarget->draw(ScreenRect(rect.x - 2, rect.y - 2, 1, rect.height + 4), inner2);
    m_renderTarget->draw(ScreenRect(rect.x - 1, rect.y - 1, 1, rect.height + 2), middle2);
    m_renderTarget->draw(ScreenRect(rect.x,     rect.y,     1, rect.height),      outer2);

    // Right border
    m_renderTarget->draw(ScreenRect(rect.x + rect.width - 1, rect.y,     1, rect.height),     outer1);
    m_renderTarget->draw(ScreenRect(rect.x + rect.width,     rect.y - 1, 1, rect.height + 2), middle1);
    m_renderTarget->draw(ScreenRect(rect.x + rect.width + 1, rect.y - 2, 1, rect.height + 4), inner1);

    // Text — uses cached SDL texture (re-renders only when string changes)
    m_text->string = text;
    m_text->color = screen->m_textFillColor;
    m_text->outlineColor = screen->m_textOutlineColor;

    Size textSize = m_text->size();
    float tx = rect.x + (rect.width - textSize.width) / 2.f;
    float ty = rect.y + (rect.height - textSize.height) / 2.f;
    if (pressed) {
        tx += screen->m_pressOffset;
        ty += screen->m_pressOffset;
    }
    m_text->position = ScreenPos(tx, ty);

    m_renderTarget->draw(m_text);
}

#else // SFML

static sf::Color toSfColor(const Drawable::Color &c) {
    return sf::Color(c.r, c.g, c.b, c.a);
}

TextButton::TextButton()
{
    m_text.setCharacterSize(17);
    m_text.setOutlineThickness(1);
    m_text.setFont(SfmlRenderTarget::plainFont());

}

// SFML is fucking shit
void TextButton::drawLine(const ScreenPos &from, const ScreenPos &to, const sf::Color &color, UiScreen *screen)
{
    sf::RectangleShape shape;
    if (from.x != to.x) {
        shape.setSize(Size(to.x - from.x, 1));
    } else {
        shape.setSize(Size(1, to.y - from.y));
    }

    shape.setPosition(from.x, from.y);

    shape.setFillColor(color);
    screen->m_renderWindow->draw(shape);
}

void TextButton::render(UiScreen *screen)
{
    /////////////////////////////////
    /// Render background and borders
    sf::RectangleShape background;
    background.setFillColor(sf::Color(0, 0, 0, screen->m_buttonOpacity * 255));
    background.setSize(rect.size());
    background.setPosition(rect.topLeft());
    screen->m_renderWindow->draw(background);

    sf::Color outer1 = toSfColor(screen->m_bevelColor1c);
    sf::Color middle1 = toSfColor(screen->m_bevelColor1b);
    sf::Color inner1 = toSfColor(screen->m_bevelColor1a);

    sf::Color outer2 = toSfColor(screen->m_bevelColor2c);
    sf::Color middle2 = toSfColor(screen->m_bevelColor2b);
    sf::Color inner2 = toSfColor(screen->m_bevelColor2a);
    if (pressed) {
        std::swap(outer1, outer2);
        std::swap(middle1, middle2);
        std::swap(inner1, inner2);
    }

    // top horizontal
    drawLine(rect.topLeft() - ScreenPos(2, 2), rect.topRight() - ScreenPos(-2, 2), inner1, screen);
    drawLine(rect.topLeft() - ScreenPos(1, 1), rect.topRight() - ScreenPos(-1, 1), middle1, screen);
    drawLine(rect.topLeft() - ScreenPos(0, 0), rect.topRight() - ScreenPos(-0, 0), outer1, screen);

    // bottom horizontal
    drawLine(rect.bottomLeft() + ScreenPos(-2, 2), rect.bottomRight() + ScreenPos(2, 2), inner2, screen);
    drawLine(rect.bottomLeft() + ScreenPos(-1, 1), rect.bottomRight() + ScreenPos(1, 1), middle2, screen);
    drawLine(rect.bottomLeft() + ScreenPos(-0, 0), rect.bottomRight() + ScreenPos(0, 0), outer2, screen);

    // left vertical
    drawLine(rect.topLeft() - ScreenPos(2, 2), rect.bottomLeft() - ScreenPos(2, -2), inner2, screen);
    drawLine(rect.topLeft() - ScreenPos(1, 1), rect.bottomLeft() - ScreenPos(1, -1), middle2, screen);
    drawLine(rect.topLeft() - ScreenPos(0, 0), rect.bottomLeft() - ScreenPos(0, -0), outer2, screen);

    // right vertical
    drawLine(rect.topRight() + ScreenPos(-1, -0), rect.bottomRight() + ScreenPos(-1, 0), outer1, screen);
    drawLine(rect.topRight() + ScreenPos( 0, -1), rect.bottomRight() + ScreenPos( 0, 1), middle1, screen);
    drawLine(rect.topRight() + ScreenPos( 1, -2), rect.bottomRight() + ScreenPos( 1, 2), inner1, screen);

    ///////////////
    /// Render text
    m_text.setFillColor(toSfColor(screen->m_textFillColor));
    m_text.setOutlineColor(toSfColor(screen->m_textOutlineColor));
    m_text.setString(text);

    ScreenPos textPosition = rect.center();
    const sf::FloatRect textRect = m_text.getLocalBounds();
    textPosition.x -= textRect.width / 2;
    textPosition.y -= 3 * textRect.height / 4;

    if (pressed) {
        textPosition.x += screen->m_pressOffset;
        textPosition.y += screen->m_pressOffset;
    }

    m_text.setPosition(textPosition);

    screen->m_renderWindow->draw(m_text);
}
#endif // !USE_SDL2
