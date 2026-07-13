#include "RandomMapSetup.h"

#include "render/SdlRenderTarget.h"
#include "core/Logger.h"

#include <thread>
#include <chrono>
#include <algorithm>

constexpr const char *RandomMapSetup::MAP_TYPE_NAMES[];
constexpr const char *RandomMapSetup::MAP_SIZE_NAMES[];
constexpr int RandomMapSetup::MAP_SIZES[];

RandomMapSetup::Result RandomMapSetup::show(SdlWindow *window,
                                             const std::shared_ptr<IRenderTarget> &renderTarget)
{
    RandomMapSetup setup;
    setup.m_window = window;
    setup.m_renderTarget = renderTarget;

    setup.m_titleText = renderTarget->createText(Drawable::Text::UI);
    setup.m_titleText->pointSize = 24;
    setup.m_titleText->color = Drawable::Color(255, 220, 150, 255);

    setup.m_labelText = renderTarget->createText(Drawable::Text::Plain);
    setup.m_labelText->pointSize = 18;

    while (window->isOpen() && !setup.m_done && !setup.m_cancelled) {
        input::Event event;
        while (window->pollEvent(event)) {
            if (event.type == input::Event::Closed) {
                setup.m_cancelled = true;
                break;
            }
            setup.handleEvent(event);
        }
        setup.render();
        window->display();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    Result result;
    result.start = setup.m_done;
    result.mapType = setup.m_mapType;
    result.mapSize = MAP_SIZES[setup.m_mapSize];
    result.playerCount = setup.m_playerCount;
    return result;
}

void RandomMapSetup::render()
{
    m_renderTarget->clear(Drawable::Color(25, 18, 8, 255));

    Size screenSize = m_renderTarget->getSize();
    float centerX = screenSize.width / 2;

    // Title
    m_titleText->string = "Random Map";
    m_titleText->position = ScreenPos(centerX - 80, 30);
    m_renderTarget->draw(m_titleText);

    float y = 90;
    float optionW = 400;
    float optionX = centerX - optionW / 2;
    float rowH = 65;
    float btnW = 50;
    float btnH = 40;

    // Map Type
    {
        m_labelText->string = "Map Type";
        m_labelText->color = Drawable::Color(180, 160, 120, 255);
        m_labelText->position = ScreenPos(optionX, y);
        m_renderTarget->draw(m_labelText);

        // < button
        m_renderTarget->draw(ScreenRect(optionX + 160, y - 2, btnW, btnH),
            Drawable::Color(60, 45, 20, 220));
        auto ltText = m_renderTarget->createText(Drawable::Text::Plain);
        ltText->string = "<"; ltText->pointSize = 20;
        ltText->color = Drawable::White;
        ltText->position = ScreenPos(optionX + 178, y + 4);
        m_renderTarget->draw(ltText);

        // Value
        m_labelText->string = MAP_TYPE_NAMES[m_mapType];
        m_labelText->color = Drawable::White;
        m_labelText->position = ScreenPos(optionX + 220, y + 2);
        m_renderTarget->draw(m_labelText);

        // > button
        m_renderTarget->draw(ScreenRect(optionX + optionW - btnW, y - 2, btnW, btnH),
            Drawable::Color(60, 45, 20, 220));
        auto gtText = m_renderTarget->createText(Drawable::Text::Plain);
        gtText->string = ">"; gtText->pointSize = 20;
        gtText->color = Drawable::White;
        gtText->position = ScreenPos(optionX + optionW - btnW + 18, y + 4);
        m_renderTarget->draw(gtText);
    }
    y += rowH;

    // Map Size
    {
        m_labelText->string = "Map Size";
        m_labelText->color = Drawable::Color(180, 160, 120, 255);
        m_labelText->position = ScreenPos(optionX, y);
        m_renderTarget->draw(m_labelText);

        m_renderTarget->draw(ScreenRect(optionX + 160, y - 2, btnW, btnH),
            Drawable::Color(60, 45, 20, 220));
        auto ltText = m_renderTarget->createText(Drawable::Text::Plain);
        ltText->string = "<"; ltText->pointSize = 20;
        ltText->color = Drawable::White;
        ltText->position = ScreenPos(optionX + 178, y + 4);
        m_renderTarget->draw(ltText);

        m_labelText->string = MAP_SIZE_NAMES[m_mapSize];
        m_labelText->color = Drawable::White;
        m_labelText->position = ScreenPos(optionX + 220, y + 2);
        m_renderTarget->draw(m_labelText);

        m_renderTarget->draw(ScreenRect(optionX + optionW - btnW, y - 2, btnW, btnH),
            Drawable::Color(60, 45, 20, 220));
        auto gtText = m_renderTarget->createText(Drawable::Text::Plain);
        gtText->string = ">"; gtText->pointSize = 20;
        gtText->color = Drawable::White;
        gtText->position = ScreenPos(optionX + optionW - btnW + 18, y + 4);
        m_renderTarget->draw(gtText);
    }
    y += rowH;

    // Player Count
    {
        m_labelText->string = "Players";
        m_labelText->color = Drawable::Color(180, 160, 120, 255);
        m_labelText->position = ScreenPos(optionX, y);
        m_renderTarget->draw(m_labelText);

        m_renderTarget->draw(ScreenRect(optionX + 160, y - 2, btnW, btnH),
            Drawable::Color(60, 45, 20, 220));
        auto ltText = m_renderTarget->createText(Drawable::Text::Plain);
        ltText->string = "<"; ltText->pointSize = 20;
        ltText->color = Drawable::White;
        ltText->position = ScreenPos(optionX + 178, y + 4);
        m_renderTarget->draw(ltText);

        m_labelText->string = std::to_string(m_playerCount);
        m_labelText->color = Drawable::White;
        m_labelText->position = ScreenPos(optionX + 250, y + 2);
        m_renderTarget->draw(m_labelText);

        m_renderTarget->draw(ScreenRect(optionX + optionW - btnW, y - 2, btnW, btnH),
            Drawable::Color(60, 45, 20, 220));
        auto gtText = m_renderTarget->createText(Drawable::Text::Plain);
        gtText->string = ">"; gtText->pointSize = 20;
        gtText->color = Drawable::White;
        gtText->position = ScreenPos(optionX + optionW - btnW + 18, y + 4);
        m_renderTarget->draw(gtText);
    }
    y += rowH + 20;

    // Start button
    float startW = 200, startH = 50;
    float startX = centerX - startW / 2;
    m_renderTarget->draw(ScreenRect(startX, y, startW, startH),
        Drawable::Color(40, 100, 40, 230));
    m_renderTarget->draw(ScreenRect(startX, y, startW, startH),
        Drawable::Transparent, Drawable::Color(80, 160, 80, 255));
    auto startText = m_renderTarget->createText(Drawable::Text::UI);
    startText->string = "Start Game";
    startText->pointSize = 20;
    startText->color = Drawable::White;
    startText->position = ScreenPos(startX + 30, y + 12);
    m_renderTarget->draw(startText);

    // Back button
    y += startH + 15;
    float backW = 150, backH = 40;
    float backX = centerX - backW / 2;
    m_renderTarget->draw(ScreenRect(backX, y, backW, backH),
        Drawable::Color(80, 30, 30, 200));
    auto backText = m_renderTarget->createText(Drawable::Text::Plain);
    backText->string = "Back";
    backText->pointSize = 16;
    backText->color = Drawable::Color(220, 200, 160, 255);
    backText->position = ScreenPos(backX + 55, y + 10);
    m_renderTarget->draw(backText);
}

void RandomMapSetup::handleEvent(const input::Event &event)
{
    if (event.type != input::Event::TouchEnded && event.type != input::Event::MouseButtonReleased)
        return;

    float x, y;
    if (event.type == input::Event::TouchEnded) {
        x = event.touch.x; y = event.touch.y;
    } else {
        x = event.mouseButton.x; y = event.mouseButton.y;
    }

    Size screenSize = m_renderTarget->getSize();
    float centerX = screenSize.width / 2;
    float optionW = 400;
    float optionX = centerX - optionW / 2;
    float btnW = 50, btnH = 40;

    // Row positions
    float row0Y = 90, rowH = 65;

    // Map Type < >
    if (y >= row0Y - 5 && y <= row0Y + btnH + 5) {
        if (x >= optionX + 160 && x <= optionX + 160 + btnW) {
            m_mapType = (m_mapType - 1 + MAP_TYPE_COUNT) % MAP_TYPE_COUNT;
        }
        if (x >= optionX + optionW - btnW && x <= optionX + optionW) {
            m_mapType = (m_mapType + 1) % MAP_TYPE_COUNT;
        }
    }

    // Map Size < >
    float row1Y = row0Y + rowH;
    if (y >= row1Y - 5 && y <= row1Y + btnH + 5) {
        if (x >= optionX + 160 && x <= optionX + 160 + btnW) {
            m_mapSize = std::max(0, m_mapSize - 1);
        }
        if (x >= optionX + optionW - btnW && x <= optionX + optionW) {
            m_mapSize = std::min(MAP_SIZE_COUNT - 1, m_mapSize + 1);
        }
    }

    // Player Count < >
    float row2Y = row1Y + rowH;
    if (y >= row2Y - 5 && y <= row2Y + btnH + 5) {
        if (x >= optionX + 160 && x <= optionX + 160 + btnW) {
            m_playerCount = std::max(2, m_playerCount - 1);
        }
        if (x >= optionX + optionW - btnW && x <= optionX + optionW) {
            m_playerCount = std::min(8, m_playerCount + 1);
        }
    }

    // Start button
    float startY = row2Y + rowH + 20;
    float startW = 200, startH = 50;
    float startX = centerX - startW / 2;
    if (x >= startX && x <= startX + startW && y >= startY && y <= startY + startH) {
        m_done = true;
    }

    // Back button
    float backY = startY + startH + 15;
    float backW = 150, backH = 40;
    float backX = centerX - backW / 2;
    if (x >= backX && x <= backX + backW && y >= backY && y <= backY + backH) {
        m_cancelled = true;
    }
}
