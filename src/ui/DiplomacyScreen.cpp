#include "DiplomacyScreen.h"

#include "mechanics/GameState.h"
#include "mechanics/Player.h"
#include "core/Logger.h"

DiplomacyScreen::DiplomacyScreen(const std::shared_ptr<IRenderTarget> &renderTarget)
    : m_renderTarget(renderTarget)
{
    m_titleText = renderTarget->createText(Drawable::Text::UI);
    m_titleText->pointSize = 24;
    m_titleText->color = Drawable::Color(255, 220, 150, 255);

    m_itemText = renderTarget->createText(Drawable::Text::Plain);
    m_itemText->pointSize = 16;
}

void DiplomacyScreen::show(const std::shared_ptr<GameState> &state)
{
    m_state = state;
    m_visible = true;
}

void DiplomacyScreen::hide()
{
    m_visible = false;
}

void DiplomacyScreen::render()
{
    if (!m_visible || !m_state) return;

    Size screenSize = m_renderTarget->getSize();

    // Semi-transparent overlay
    m_renderTarget->draw(ScreenRect(0, 0, screenSize.width, screenSize.height),
        Drawable::Color(0, 0, 0, 180));

    // Panel background
    float panelW = std::min(600.f, screenSize.width - 40.f);
    float panelH = std::min(400.f, screenSize.height - 80.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Color(35, 25, 12, 245));
    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

    // Title
    m_titleText->string = "Diplomacy";
    m_titleText->position = ScreenPos(panelX + 20, panelY + 15);
    m_renderTarget->draw(m_titleText);

    // Close button
    float closeX = panelX + panelW - 40;
    float closeY = panelY + 10;
    m_renderTarget->draw(ScreenRect(closeX, closeY, 30, 30),
        Drawable::Color(80, 30, 30, 200));
    auto closeText = m_renderTarget->createText(Drawable::Text::Plain);
    closeText->string = "X";
    closeText->pointSize = 18;
    closeText->color = Drawable::White;
    closeText->position = ScreenPos(closeX + 8, closeY + 4);
    m_renderTarget->draw(closeText);

    // Player rows
    Player::Ptr humanPlayer = m_state->humanPlayer();
    if (!humanPlayer) return;

    float rowY = panelY + 55;
    float rowH = 42;

    // Player colors for display
    static const Drawable::Color playerColors[] = {
        Drawable::Color(0, 0, 200, 255),     // Blue
        Drawable::Color(200, 0, 0, 255),     // Red
        Drawable::Color(0, 200, 0, 255),     // Green
        Drawable::Color(200, 200, 0, 255),   // Yellow
        Drawable::Color(0, 200, 200, 255),   // Cyan
        Drawable::Color(200, 0, 200, 255),   // Purple
        Drawable::Color(128, 128, 128, 255), // Gray
        Drawable::Color(200, 128, 0, 255),   // Orange
    };

    for (const Player::Ptr &player : m_state->players()) {
        if (!player) continue;
        if (player->playerId == 0) continue; // skip Gaia
        if (player == humanPlayer) continue;  // skip self

        if (rowY + rowH > panelY + panelH - 10) break;

        // Player color indicator
        int colorIdx = (player->playerId - 1) % 8;
        m_renderTarget->draw(ScreenRect(panelX + 20, rowY + 8, 24, 24), playerColors[colorIdx]);

        // Player name
        m_itemText->string = "Player " + std::to_string(player->playerId);
        m_itemText->color = Drawable::White;
        m_itemText->position = ScreenPos(panelX + 55, rowY + 10);
        m_renderTarget->draw(m_itemText);

        // Stance buttons
        float btnX = panelX + 200;
        float btnW = 80;
        float btnH = 30;
        float btnGap = 8;

        auto currentStance = humanPlayer->diplomaticStanceTo(player->playerId);

        // Ally button
        bool isAlly = (currentStance == Player::DiplomaticStance::Allied);
        m_renderTarget->draw(ScreenRect(btnX, rowY + 5, btnW, btnH),
            isAlly ? Drawable::Color(0, 100, 0, 220) : Drawable::Color(50, 40, 25, 200));
        auto allyText = m_renderTarget->createText(Drawable::Text::Plain);
        allyText->string = "Ally";
        allyText->pointSize = 14;
        allyText->color = isAlly ? Drawable::Color(150, 255, 150, 255) : Drawable::Color(180, 160, 120, 255);
        allyText->position = ScreenPos(btnX + 20, rowY + 10);
        m_renderTarget->draw(allyText);

        // Neutral button
        btnX += btnW + btnGap;
        bool isNeutral = (currentStance == Player::DiplomaticStance::Neutral);
        m_renderTarget->draw(ScreenRect(btnX, rowY + 5, btnW, btnH),
            isNeutral ? Drawable::Color(100, 100, 0, 220) : Drawable::Color(50, 40, 25, 200));
        auto neutralText = m_renderTarget->createText(Drawable::Text::Plain);
        neutralText->string = "Neutral";
        neutralText->pointSize = 14;
        neutralText->color = isNeutral ? Drawable::Color(255, 255, 150, 255) : Drawable::Color(180, 160, 120, 255);
        neutralText->position = ScreenPos(btnX + 10, rowY + 10);
        m_renderTarget->draw(neutralText);

        // Enemy button
        btnX += btnW + btnGap;
        bool isEnemy = (currentStance == Player::DiplomaticStance::Enemy);
        m_renderTarget->draw(ScreenRect(btnX, rowY + 5, btnW, btnH),
            isEnemy ? Drawable::Color(100, 0, 0, 220) : Drawable::Color(50, 40, 25, 200));
        auto enemyText = m_renderTarget->createText(Drawable::Text::Plain);
        enemyText->string = "Enemy";
        enemyText->pointSize = 14;
        enemyText->color = isEnemy ? Drawable::Color(255, 150, 150, 255) : Drawable::Color(180, 160, 120, 255);
        enemyText->position = ScreenPos(btnX + 15, rowY + 10);
        m_renderTarget->draw(enemyText);

        rowY += rowH;
    }
}

bool DiplomacyScreen::handleEvent(const input::Event &event)
{
    if (!m_visible || !m_state) return false;

    ScreenPos pos;
    bool isTap = false;

    if (event.type == input::Event::TouchEnded) {
        pos = ScreenPos(event.touch.x, event.touch.y);
        isTap = true;
    } else if (event.type == input::Event::MouseButtonReleased) {
        pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isTap = true;
    }

    // Escape closes the screen
    if (event.type == input::Event::KeyPressed && event.key.code == input::Key::Escape) {
        hide();
        return true;
    }

    if (!isTap) return true; // consume all events while visible

    Size screenSize = m_renderTarget->getSize();
    float panelW = std::min(600.f, screenSize.width - 40.f);
    float panelH = std::min(400.f, screenSize.height - 80.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    // Close button check
    ScreenRect closeRect(panelX + panelW - 40, panelY + 10, 30, 30);
    if (closeRect.contains(pos)) {
        hide();
        return true;
    }

    // Click outside panel = close
    ScreenRect panelRect(panelX, panelY, panelW, panelH);
    if (!panelRect.contains(pos)) {
        hide();
        return true;
    }

    // Check stance button clicks
    Player::Ptr humanPlayer = m_state->humanPlayer();
    if (!humanPlayer) return true;

    float rowY = panelY + 55;
    float rowH = 42;

    for (const Player::Ptr &player : m_state->players()) {
        if (!player) continue;
        if (player->playerId == 0) continue;
        if (player == humanPlayer) continue;

        float btnX = panelX + 200;
        float btnW = 80;
        float btnH = 30;
        float btnGap = 8;

        // Ally
        if (ScreenRect(btnX, rowY + 5, btnW, btnH).contains(pos)) {
            humanPlayer->setDiplomaticStance(player->playerId, Player::DiplomaticStance::Allied, player.get());
            return true;
        }
        btnX += btnW + btnGap;
        // Neutral
        if (ScreenRect(btnX, rowY + 5, btnW, btnH).contains(pos)) {
            humanPlayer->setDiplomaticStance(player->playerId, Player::DiplomaticStance::Neutral);
            return true;
        }
        btnX += btnW + btnGap;
        // Enemy
        if (ScreenRect(btnX, rowY + 5, btnW, btnH).contains(pos)) {
            humanPlayer->setDiplomaticStance(player->playerId, Player::DiplomaticStance::Enemy);
            return true;
        }

        rowY += rowH;
    }

    return true;
}
