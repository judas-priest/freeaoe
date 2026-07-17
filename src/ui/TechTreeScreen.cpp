#include "TechTreeScreen.h"

#include "mechanics/GameState.h"
#include "mechanics/Player.h"
#include "core/Logger.h"

#include <genie/dat/Research.h>

#include <algorithm>

TechTreeScreen::TechTreeScreen(const std::shared_ptr<IRenderTarget> &renderTarget)
    : m_renderTarget(renderTarget)
{
    m_titleText = renderTarget->createText(Drawable::Text::UI);
    m_titleText->pointSize = 22;
    m_titleText->color = Drawable::Color(255, 220, 150, 255);

    m_itemText = renderTarget->createText(Drawable::Text::Plain);
    m_itemText->pointSize = 13;
}

void TechTreeScreen::show(const std::shared_ptr<GameState> &state)
{
    m_state = state;
    m_visible = true;
    m_scrollX = 0;
    buildTechList();
}

void TechTreeScreen::hide()
{
    m_visible = false;
}

void TechTreeScreen::buildTechList()
{
    m_techs.clear();
    if (!m_state) return;

    const Player::Ptr &human = m_state->humanPlayer();
    if (!human) return;

    // Get civ-specific techs (only those not disabled by TechTreeID)
    const auto &civTechs = human->civilization.availableTechs();

    for (const auto &[techId, tech] : civTechs) {
        if (tech.Name.empty() || tech.Name[0] == '\0') continue;
        if (tech.ResearchTime < 0) continue; // not a real research

        TechEntry entry;
        entry.name = tech.Name;
        // Clean up null chars
        entry.name.erase(std::find(entry.name.begin(), entry.name.end(), '\0'), entry.name.end());
        if (entry.name.empty()) continue;

        // Determine age based on required tech
        entry.age = 0;
        if (tech.RequiredTechCount > 0 && tech.RequiredTechs[0] >= 0) {
            // Rough age estimation: check required tech chain
            int reqTech = tech.RequiredTechs[0];
            if (reqTech == 22) entry.age = 1; // Feudal
            else if (reqTech == 23) entry.age = 2; // Castle
            else if (reqTech == 24) entry.age = 3; // Imperial
            else if (reqTech >= 100) entry.age = 2; // Most advanced techs
            else entry.age = 1;
        }

        entry.available = human->researchAvailable(techId);
        entry.researched = human->hasResearched(techId);

        m_techs.push_back(std::move(entry));
    }

    // Sort by age, then name
    std::sort(m_techs.begin(), m_techs.end(), [](const TechEntry &a, const TechEntry &b) {
        if (a.age != b.age) return a.age < b.age;
        return a.name < b.name;
    });
}

void TechTreeScreen::render()
{
    if (!m_visible) return;

    Size screenSize = m_renderTarget->getSize();

    // Full screen overlay
    m_renderTarget->draw(ScreenRect(0, 0, screenSize.width, screenSize.height),
        Drawable::Color(0, 0, 0, 200));

    // Panel
    float panelX = 20;
    float panelY = 20;
    float panelW = screenSize.width - 40;
    float panelH = screenSize.height - 40;

    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Color(30, 22, 10, 245));
    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

    // Title
    if (m_state && m_state->humanPlayer()) {
        m_titleText->string = "Technology Tree - " + m_state->humanPlayer()->civilization.name();
    } else {
        m_titleText->string = "Technology Tree";
    }
    m_titleText->position = ScreenPos(panelX + 15, panelY + 10);
    m_renderTarget->draw(m_titleText);

    // Close button
    float closeX = panelX + panelW - 35;
    float closeY = panelY + 8;
    m_renderTarget->draw(ScreenRect(closeX, closeY, 28, 28),
        Drawable::Color(80, 30, 30, 200));
    auto closeText = m_renderTarget->createText(Drawable::Text::Plain);
    closeText->string = "X";
    closeText->pointSize = 16;
    closeText->color = Drawable::White;
    closeText->position = ScreenPos(closeX + 7, closeY + 4);
    m_renderTarget->draw(closeText);

    // Age column headers
    static const char *ageNames[] = {"Dark Age", "Feudal Age", "Castle Age", "Imperial Age"};
    float colW = (panelW - 20) / 4.f;
    for (int age = 0; age < 4; age++) {
        float cx = panelX + 10 + age * colW;
        // Header background
        m_renderTarget->draw(ScreenRect(cx, panelY + 42, colW - 4, 22),
            Drawable::Color(50, 38, 18, 220));
        auto headerText = m_renderTarget->createText(Drawable::Text::Plain);
        headerText->string = ageNames[age];
        headerText->pointSize = 13;
        headerText->color = Drawable::Color(220, 200, 150, 255);
        headerText->position = ScreenPos(cx + 5, panelY + 45);
        m_renderTarget->draw(headerText);
    }

    // Tech entries per column
    float entryH = 20;
    int perCol[4] = {0, 0, 0, 0};

    for (const TechEntry &tech : m_techs) {
        int age = std::clamp(tech.age, 0, 3);
        float cx = panelX + 10 + age * colW;
        float cy = panelY + 68 + perCol[age] * entryH;

        if (cy + entryH > panelY + panelH - 10) continue; // clip

        // Color based on state
        Drawable::Color textColor;
        if (tech.researched) {
            textColor = Drawable::Color(100, 220, 100, 255); // Green = done
        } else if (tech.available) {
            textColor = Drawable::Color(220, 220, 220, 255); // White = available
        } else {
            textColor = Drawable::Color(120, 100, 80, 180); // Dim = unavailable
        }

        m_itemText->string = tech.name;
        m_itemText->color = textColor;
        m_itemText->position = ScreenPos(cx + 5, cy);
        m_renderTarget->draw(m_itemText);

        perCol[age]++;
    }
}

bool TechTreeScreen::handleEvent(const input::Event &event)
{
    if (!m_visible) return false;

    ScreenPos pos;
    bool isTap = false;

    if (event.type == input::Event::TouchEnded || event.type == input::Event::MouseButtonReleased) {
        pos = (event.type == input::Event::TouchEnded)
            ? ScreenPos(event.touch.x, event.touch.y)
            : ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isTap = true;
    }

    if (!isTap) return true; // consume all events

    Size screenSize = m_renderTarget->getSize();
    float panelX = 20;
    float panelY = 20;
    float panelW = screenSize.width - 40;

    // Close button
    if (ScreenRect(panelX + panelW - 35, panelY + 8, 28, 28).contains(pos)) {
        hide();
        return true;
    }

    // Tap outside panel = close
    if (pos.x < panelX || pos.x > panelX + panelW ||
        pos.y < panelY || pos.y > panelY + screenSize.height - 40) {
        hide();
        return true;
    }

    return true;
}
