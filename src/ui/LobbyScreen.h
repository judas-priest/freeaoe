#pragma once

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"
#include "core/Types.h"

#include <memory>
#include <string>
#include <vector>

class NetHost;
class NetClient;

/// Multiplayer lobby screen overlay (host or join mode).
class LobbyScreen
{
public:
    LobbyScreen(const std::shared_ptr<IRenderTarget> &renderTarget);

    /// Show the lobby. If isHost is true, starts hosting; otherwise joins.
    void show(bool isHost, const std::string &address = "127.0.0.1", int port = 12345);
    void hide();
    bool isVisible() const { return m_visible; }

    void render();
    bool handleEvent(const input::Event &event);

    /// True when host clicked "Start Game" or client received start signal.
    bool isGameStartRequested() const { return m_startRequested; }
    void clearStartRequest() { m_startRequested = false; }

    bool isHosting() const { return m_isHost; }
    const std::shared_ptr<NetHost> &host() const { return m_host; }
    const std::shared_ptr<NetClient> &client() const { return m_client; }

    struct PlayerEntry {
        std::string name;
        int civId = 1;       // 1-based civ index
        bool ready = false;
    };

private:
    std::shared_ptr<IRenderTarget> m_renderTarget;
    bool m_visible = false;
    bool m_isHost = false;
    bool m_startRequested = false;

    std::string m_address;
    int m_port = 12345;
    std::string m_statusText;

    std::shared_ptr<NetHost> m_host;
    std::shared_ptr<NetClient> m_client;

    std::vector<PlayerEntry> m_players;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_labelText;

    // Input field for join address
    std::string m_joinAddress = "127.0.0.1";
    bool m_editingAddress = false;
};
