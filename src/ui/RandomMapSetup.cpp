#include "RandomMapSetup.h"

#include "render/SdlRenderTarget.h"
#include "resource/DataManager.h"
#include "core/Logger.h"

#include <thread>
#include <chrono>
#include <algorithm>

constexpr const char *RandomMapSetup::MAP_TYPE_NAMES[];
constexpr const char *RandomMapSetup::MAP_SIZE_NAMES[];
constexpr int RandomMapSetup::MAP_SIZES[];
constexpr const char *RandomMapSetup::AGE_NAMES[];

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

    // Load civilization names
    const auto &civs = DataManager::Inst().civilizations();
    setup.m_civNames.push_back("Random");
    for (const auto &civ : civs) {
        setup.m_civNames.push_back(civ.Name);
    }

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
    result.startingAge = setup.m_startingAge;
    for (int i = 0; i < 8; i++) {
        result.civIds[i] = setup.m_civIds[i];
        result.teams[i] = setup.m_teams[i];
    }
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
    y += rowH;

    // Starting Age
    {
        m_labelText->string = "Starting Age";
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

        m_labelText->string = AGE_NAMES[m_startingAge];
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

    // Per-player rows: Civ and Team
    int civCount = static_cast<int>(m_civNames.size());
    float playerRowH = 50;
    for (int p = 0; p < m_playerCount; p++) {
        // Player label
        m_labelText->string = (p == 0) ? "You" : ("AI " + std::to_string(p + 1));
        m_labelText->color = Drawable::Color(180, 160, 120, 255);
        m_labelText->position = ScreenPos(optionX, y);
        m_renderTarget->draw(m_labelText);

        // Civ selector: < CivName >
        float civX = optionX + 80;
        float civW = 200;
        m_renderTarget->draw(ScreenRect(civX, y - 2, 30, 34),
            Drawable::Color(60, 45, 20, 220));
        auto civLt = m_renderTarget->createText(Drawable::Text::Plain);
        civLt->string = "<"; civLt->pointSize = 16;
        civLt->color = Drawable::White;
        civLt->position = ScreenPos(civX + 10, y + 4);
        m_renderTarget->draw(civLt);

        m_labelText->string = m_civNames[m_civIds[p]];
        m_labelText->color = Drawable::White;
        m_labelText->position = ScreenPos(civX + 38, y + 2);
        m_renderTarget->draw(m_labelText);

        m_renderTarget->draw(ScreenRect(civX + civW - 30, y - 2, 30, 34),
            Drawable::Color(60, 45, 20, 220));
        auto civGt = m_renderTarget->createText(Drawable::Text::Plain);
        civGt->string = ">"; civGt->pointSize = 16;
        civGt->color = Drawable::White;
        civGt->position = ScreenPos(civX + civW - 20, y + 4);
        m_renderTarget->draw(civGt);

        // Team selector: Tm < N >
        float tmX = optionX + 300;
        m_labelText->string = "Tm";
        m_labelText->color = Drawable::Color(140, 130, 100, 255);
        m_labelText->position = ScreenPos(tmX, y + 2);
        m_renderTarget->draw(m_labelText);

        float tmBtnX = tmX + 30;
        m_renderTarget->draw(ScreenRect(tmBtnX, y - 2, 30, 34),
            Drawable::Color(60, 45, 20, 220));
        auto tmLt = m_renderTarget->createText(Drawable::Text::Plain);
        tmLt->string = "<"; tmLt->pointSize = 16;
        tmLt->color = Drawable::White;
        tmLt->position = ScreenPos(tmBtnX + 10, y + 4);
        m_renderTarget->draw(tmLt);

        m_labelText->string = (m_teams[p] == 0) ? "-" : std::to_string(m_teams[p]);
        m_labelText->color = Drawable::White;
        m_labelText->position = ScreenPos(tmBtnX + 38, y + 2);
        m_renderTarget->draw(m_labelText);

        m_renderTarget->draw(ScreenRect(tmBtnX + 60, y - 2, 30, 34),
            Drawable::Color(60, 45, 20, 220));
        auto tmGt = m_renderTarget->createText(Drawable::Text::Plain);
        tmGt->string = ">"; tmGt->pointSize = 16;
        tmGt->color = Drawable::White;
        tmGt->position = ScreenPos(tmBtnX + 70, y + 4);
        m_renderTarget->draw(tmGt);

        y += playerRowH;
    }
    y += 15;

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

    // Starting Age < >
    float row3Y = row2Y + rowH;
    if (y >= row3Y - 5 && y <= row3Y + btnH + 5) {
        if (x >= optionX + 160 && x <= optionX + 160 + btnW) {
            m_startingAge = (m_startingAge - 1 + AGE_COUNT) % AGE_COUNT;
        }
        if (x >= optionX + optionW - btnW && x <= optionX + optionW) {
            m_startingAge = (m_startingAge + 1) % AGE_COUNT;
        }
    }

    // Per-player rows: Civ and Team
    int civCount = static_cast<int>(m_civNames.size());
    float playerRowH = 50;
    float playerStartY = row3Y + rowH;
    for (int p = 0; p < m_playerCount; p++) {
        float rowY = playerStartY + p * playerRowH;
        float civX = optionX + 80;
        float civW = 200;
        float civBtnW = 30, civBtnH = 34;

        if (y >= rowY - 5 && y <= rowY + civBtnH + 5) {
            // Civ < button
            if (x >= civX && x <= civX + civBtnW) {
                m_civIds[p] = (m_civIds[p] - 1 + civCount) % civCount;
            }
            // Civ > button
            if (x >= civX + civW - civBtnW && x <= civX + civW) {
                m_civIds[p] = (m_civIds[p] + 1) % civCount;
            }

            // Team < button
            float tmBtnX = optionX + 300 + 30;
            if (x >= tmBtnX && x <= tmBtnX + civBtnW) {
                m_teams[p] = (m_teams[p] - 1 + (MAX_TEAMS + 1)) % (MAX_TEAMS + 1);
            }
            // Team > button
            if (x >= tmBtnX + 60 && x <= tmBtnX + 60 + civBtnW) {
                m_teams[p] = (m_teams[p] + 1) % (MAX_TEAMS + 1);
            }
        }
    }

    // Start button
    float startY = playerStartY + m_playerCount * playerRowH + 15;
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
