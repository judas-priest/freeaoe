#include "LobbyScreen.h"

#include "core/Logger.h"
#include "net/NetHost.h"
#include "net/NetClient.h"

#include <algorithm>

LobbyScreen::LobbyScreen(const std::shared_ptr<IRenderTarget> &renderTarget)
    : m_renderTarget(renderTarget)
{
    m_titleText = renderTarget->createText(Drawable::Text::UI);
    m_titleText->pointSize = 24;
    m_titleText->color = Drawable::Color(255, 220, 150, 255);

    m_labelText = renderTarget->createText(Drawable::Text::Plain);
    m_labelText->pointSize = 16;
}

void LobbyScreen::show(bool isHost, const std::string &address, int port)
{
    m_visible = true;
    m_isHost = isHost;
    m_address = address;
    m_port = port;
    m_startRequested = false;
    m_players.clear();

    if (isHost) {
        m_statusText = "Hosting on port " + std::to_string(port) + "...";

        m_host = std::make_shared<NetHost>();
        if (m_host->start(port)) {
            m_statusText = "Hosting on port " + std::to_string(port);
        } else {
            m_statusText = "Failed to bind port " + std::to_string(port);
        }

        // Add self as player 1
        PlayerEntry self;
        self.name = "Host";
        self.ready = true;
        m_players.push_back(self);
    } else {
        m_statusText = "Connecting to " + address + ":" + std::to_string(port) + "...";

        m_client = std::make_shared<NetClient>();
        if (m_client->connect(address, port)) {
            m_statusText = "Connected to " + address;
        } else {
            m_statusText = "Failed to connect to " + address + ":" + std::to_string(port);
        }

        PlayerEntry self;
        self.name = "Client";
        self.ready = false;
        m_players.push_back(self);
    }
}

void LobbyScreen::hide()
{
    m_visible = false;
}

void LobbyScreen::render()
{
    if (!m_visible) return;

    Size screenSize = m_renderTarget->getSize();

    // Overlay
    m_renderTarget->draw(ScreenRect(0, 0, screenSize.width, screenSize.height),
        Drawable::Color(0, 0, 0, 180));

    // Panel
    float panelW = std::min(550.f, screenSize.width - 40.f);
    float panelH = std::min(420.f, screenSize.height - 80.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Color(35, 25, 12, 245));
    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

    // Title
    m_titleText->string = m_isHost ? "Host Multiplayer Game" : "Join Multiplayer Game";
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

    // Status text
    m_labelText->string = m_statusText;
    m_labelText->color = Drawable::Color(200, 200, 200, 255);
    m_labelText->position = ScreenPos(panelX + 20, panelY + 60);
    m_renderTarget->draw(m_labelText);

    // Player list header
    m_labelText->string = "Players:";
    m_labelText->color = Drawable::Color(220, 200, 160, 255);
    m_labelText->position = ScreenPos(panelX + 20, panelY + 100);
    m_renderTarget->draw(m_labelText);

    // Player entries
    for (size_t i = 0; i < m_players.size(); i++) {
        float rowY = panelY + 130 + i * 30;
        std::string playerLine = std::to_string(i + 1) + ". " + m_players[i].name;
        playerLine += "  [Civ " + std::to_string(m_players[i].civId) + "]";
        if (m_players[i].ready) {
            playerLine += "  READY";
        }

        m_labelText->string = playerLine;
        m_labelText->color = m_players[i].ready
            ? Drawable::Color(100, 220, 100, 255)
            : Drawable::Color(220, 200, 160, 255);
        m_labelText->position = ScreenPos(panelX + 30, rowY);
        m_renderTarget->draw(m_labelText);
    }

    // Bottom buttons
    float btnY = panelY + panelH - 55;

    if (m_isHost) {
        // Start Game button
        float startX = panelX + panelW / 2 - 70;
        m_renderTarget->draw(ScreenRect(startX, btnY, 140, 36),
            Drawable::Color(40, 80, 40, 230));
        auto startText = m_renderTarget->createText(Drawable::Text::Plain);
        startText->string = "Start Game";
        startText->pointSize = 16;
        startText->color = Drawable::Color(220, 255, 220, 255);
        startText->position = ScreenPos(startX + 20, btnY + 8);
        m_renderTarget->draw(startText);
    } else {
        // Ready button
        float readyX = panelX + panelW / 2 - 50;
        bool selfReady = !m_players.empty() && m_players[0].ready;
        m_renderTarget->draw(ScreenRect(readyX, btnY, 100, 36),
            selfReady
                ? Drawable::Color(40, 80, 40, 230)
                : Drawable::Color(60, 45, 20, 230));
        auto readyText = m_renderTarget->createText(Drawable::Text::Plain);
        readyText->string = selfReady ? "Ready!" : "Ready";
        readyText->pointSize = 16;
        readyText->color = Drawable::Color(220, 200, 160, 255);
        readyText->position = ScreenPos(readyX + 20, btnY + 8);
        m_renderTarget->draw(readyText);
    }

    // Cancel button
    float cancelX = panelX + 20;
    m_renderTarget->draw(ScreenRect(cancelX, btnY, 100, 36),
        Drawable::Color(80, 30, 30, 230));
    auto cancelText = m_renderTarget->createText(Drawable::Text::Plain);
    cancelText->string = "Cancel";
    cancelText->pointSize = 16;
    cancelText->color = Drawable::Color(220, 200, 160, 255);
    cancelText->position = ScreenPos(cancelX + 22, btnY + 8);
    m_renderTarget->draw(cancelText);
}

bool LobbyScreen::handleEvent(const input::Event &event)
{
    if (!m_visible) return false;

    // Escape closes
    if (event.type == input::Event::KeyPressed && event.key.code == input::Key::Escape) {
        hide();
        return true;
    }

    ScreenPos pos;
    bool isTap = false;

    if (event.type == input::Event::TouchEnded || event.type == input::Event::MouseButtonReleased) {
        pos = (event.type == input::Event::TouchEnded)
            ? ScreenPos(event.touch.x, event.touch.y)
            : ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isTap = true;
    }

    if (!isTap) return true; // consume all events while visible

    Size screenSize = m_renderTarget->getSize();
    float panelW = std::min(550.f, screenSize.width - 40.f);
    float panelH = std::min(420.f, screenSize.height - 80.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    // Close button
    if (ScreenRect(panelX + panelW - 40, panelY + 10, 30, 30).contains(pos)) {
        hide();
        return true;
    }

    float btnY = panelY + panelH - 55;

    // Cancel button
    if (ScreenRect(panelX + 20, btnY, 100, 36).contains(pos)) {
        hide();
        return true;
    }

    if (m_isHost) {
        // Start Game button
        float startX = panelX + panelW / 2 - 70;
        if (ScreenRect(startX, btnY, 140, 36).contains(pos)) {
            m_startRequested = true;
            return true;
        }
    } else {
        // Ready button
        float readyX = panelX + panelW / 2 - 50;
        if (ScreenRect(readyX, btnY, 100, 36).contains(pos)) {
            if (!m_players.empty()) {
                m_players[0].ready = !m_players[0].ready;
            }
            return true;
        }
    }

    // Click outside panel = close
    if (!ScreenRect(panelX, panelY, panelW, panelH).contains(pos)) {
        hide();
        return true;
    }

    return true;
}
