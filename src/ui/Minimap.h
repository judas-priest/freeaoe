#pragma once

#include <functional>
#include <memory>

#include "core/Types.h"
#include "core/SignalEmitter.h"
#include "global/EventListener.h"
#include "mechanics/IState.h"
#include "render/IRenderTarget.h"

class Map;
class Player;
class UnitManager;
struct Unit;
struct VisibilityMap;

class Minimap : public IState, public SignalReceiver, public EventListener
{
public:
    // TODO implement the rest
    enum class MinimapMode {
        Normal,
        Economic,
        Diplomatic
    };

    Minimap(const IRenderTargetPtr &renderTarget);

    void setMap(const std::shared_ptr<Map> &map);
    void setUnitManager(const std::shared_ptr<UnitManager> &unitManager);
    void setVisibilityMap(const std::shared_ptr<VisibilityMap> &visibilityMap);
    void setHumanPlayer(const std::shared_ptr<Player> &player);
    void setAllPlayers(const std::vector<std::shared_ptr<Player>> &players);

    bool init() override;
    bool handleEvent(input::Event event) override;
    void mouseExited() { m_mousePressed = false; }
    bool update(Time time) override;
    void draw() override;
    ScreenRect rect() const { return m_rect; }
    void cycleMode();
    MinimapMode mode() const { return m_mode; }

    void addFlare(const MapPos &position, int playerId);
    void setFlareCallback(std::function<void(const MapPos&)> callback) { m_onFlare = callback; }

private:
    void updateUnits();
    void updateTerrain();
    void updateCamera();
    bool updateRect(const Size &windowSize);

    void onTileDiscovered(const int playerID, const int tileX, const int tileY) override;
    void onTileHidden(const int playerID, const int tileX, const int tileY) override;

    Drawable::Color unitColor(const std::shared_ptr<Unit> &unit);

    bool m_unitsUpdated = false;
    bool m_terrainUpdated = false;
    std::shared_ptr<Map> m_map;
    std::shared_ptr<UnitManager> m_unitManager;
    IRenderTargetPtr m_renderTarget;
    ScreenRect m_rect;
    IRenderTargetPtr m_terrainTexture;
    IRenderTargetPtr m_unitsTexture;
    MapPos m_lastCameraPos;
    ScreenRect m_cameraRect;
    bool m_mousePressed = false;
    std::shared_ptr<VisibilityMap> m_visibilityMap;
    std::shared_ptr<Player> m_humanPlayer;
    std::vector<std::shared_ptr<Player>> m_allPlayers;

    Size m_windowSize;

    MinimapMode m_mode = MinimapMode::Diplomatic; // easiest, so sue me

    Drawable::Text::Ptr m_modeLabel;

    struct Flare {
        MapPos position;
        int playerId = 0;
        float timeLeft = 5000.f; // ms
    };
    std::vector<Flare> m_flares;
    std::function<void(const MapPos&)> m_onFlare;
    Time m_lastFlareUpdate = 0;
};

