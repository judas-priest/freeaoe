#include "Minimap.h"

#include "core/Constants.h"
#include "core/Logger.h"
#include "core/Utility.h"
#include "mechanics/Map.h"
#include "mechanics/MapTile.h"
#include "mechanics/Player.h"
#include "mechanics/Unit.h"
#include "mechanics/UnitManager.h"
#include "render/Camera.h"
#include "resource/AssetManager.h"
#include "resource/DataManager.h"

#include <genie/Types.h>
#include <genie/dat/ResourceType.h>
#include <genie/dat/Terrain.h>
#include <genie/dat/Unit.h>
#include <genie/resource/Color.h>
#include <genie/resource/PalFile.h>

#include <algorithm>
#include <functional>
#include <vector>

#include <assert.h>

Minimap::Minimap(const IRenderTargetPtr &renderTarget) :
    m_renderTarget(renderTarget)
{
    updateRect(renderTarget->getSize());
    EventManager::registerListener(this, EventManager::TileDiscovered);
    EventManager::registerListener(this, EventManager::TileHidden);
}

bool Minimap::updateRect(const Size &size)
{
    if (size == m_windowSize && m_rect.width > 0) {
        return false;
    }

    if (size.height == 1024) {
        m_rect = ScreenRect(865, 815, 400, 200);
    } else if (size.height == 768) {
        m_rect = ScreenRect(688, 599, 336, 169);
    } else if (size.height == 600) {
        m_rect = ScreenRect(510, 465, 267, 134);
    } else {
        int mmW = std::min(240, static_cast<int>(size.width * 0.18f));
        int mmH = mmW / 2;
        m_rect = ScreenRect(size.width - mmW - 8, size.height - mmH - 8, mmW, mmH);
    }

    m_windowSize = size;

    return true;
}

void Minimap::onTileDiscovered(const int playerID, const int /*tileX*/, const int /*tileY*/)
{
    if (!m_unitManager) return;
    if (playerID == m_unitManager->humanPlayerID()) {
        m_terrainUpdated = true;
    }
}

void Minimap::onTileHidden(const int playerID, const int /*tileX*/, const int /*tileY*/)
{
    if (!m_unitManager) return;
    if (playerID == m_unitManager->humanPlayerID()) {
        m_terrainUpdated = true;
    }
}

void Minimap::setMap(const std::shared_ptr<Map> &map)
{
    if (map == m_map) {
        return;
    }

    if (!map) {
        WARN << "null map";
        return;
    }

    if (m_map) {
        m_map->disconnect(this);
    }

    m_map = map;
    map->connect(Map::Signals::UnitsChanged, this, &Minimap::updateUnits);
    map->connect(Map::Signals::TerrainChanged, this, &Minimap::updateTerrain);

    m_unitsUpdated = true;

    m_lastCameraPos = MapPos(-1, -1);
    m_terrainUpdated = true;

}

void Minimap::setUnitManager(const std::shared_ptr<UnitManager> &unitManager)
{
    if (unitManager == m_unitManager) {
        return;
    }

    if (!unitManager) {
        WARN << "null unit manager";
    }

    m_unitManager = unitManager;
    m_unitsUpdated = true;
}

void Minimap::setVisibilityMap(const std::shared_ptr<VisibilityMap> &visibilityMap)
{
    if (visibilityMap == m_visibilityMap) {
        return;
    }

    m_visibilityMap = visibilityMap;
}

void Minimap::updateUnits()
{
    m_unitsUpdated = true;
}

void Minimap::updateTerrain()
{
    m_terrainUpdated = true;
}

void Minimap::updateCamera()
{
    const MapRect mapDimensions(0, 0, m_map->columnCount(), m_map->rowCount());
    const float scaleX = m_rect.boundingMapRect().width / mapDimensions.width / 2;
    const float scaleY = m_rect.boundingMapRect().height / mapDimensions.height / 2;

    const Size viewSize = m_renderTarget->camera()->m_viewportSize;
    const MapPos cameraMapPos(m_renderTarget->camera()->m_target.y / Constants::TILE_SIZE, m_renderTarget->camera()->m_target.x / Constants::TILE_SIZE);
    const ScreenPos cameraPos = cameraMapPos.toScreen();
    const ScreenRect fullBoundingRect = MapRect(0, 0, mapDimensions.width * Constants::TILE_SIZE, mapDimensions.height * Constants::TILE_SIZE).boundingScreenRect();
    const ScreenPos center(m_rect.width/2, m_rect.height/2);

    m_cameraRect.width = m_rect.width * viewSize.width / fullBoundingRect.width;
    m_cameraRect.height = m_rect.height * viewSize.height / fullBoundingRect.height;
    m_cameraRect.x = m_rect.x + cameraPos.x * scaleX - m_cameraRect.width / 2;
    m_cameraRect.y = m_rect.y + cameraPos.y * scaleY + center.y - m_cameraRect.height / 2;
}

static Drawable::Color playerMinimapColor(int playerId)
{
    // Standard AoE2 player colors (1-indexed, 0 = gaia)
    static const Drawable::Color colors[8] = {
        Drawable::Color(0,   0,   255),  // player 1 -- blue
        Drawable::Color(255, 0,   0),    // player 2 -- red
        Drawable::Color(0,   255, 0),    // player 3 -- green
        Drawable::Color(255, 255, 0),    // player 4 -- yellow
        Drawable::Color(0,   255, 255),  // player 5 -- cyan
        Drawable::Color(255, 0,   255),  // player 6 -- magenta
        Drawable::Color(128, 128, 128),  // player 7 -- grey
        Drawable::Color(255, 128, 0),    // player 8 -- orange
    };
    if (playerId >= 1 && playerId <= 8) {
        return colors[playerId - 1];
    }
    return Drawable::Color(128, 192, 128); // gaia / fallback
}

Drawable::Color Minimap::unitColor(const std::shared_ptr<Unit> &unit)
{
    if (m_unitManager->selected().contains(unit)) {
        return Drawable::White;
    }

    const bool isHuman = (unit->playerId() == m_unitManager->humanPlayerID());
    const bool isGaia  = (unit->playerId() == UnitManager::GaiaID);

    switch (m_mode) {
    case MinimapMode::Diplomatic:
    default:
        if (isGaia) return Drawable::Color(128, 192, 128);
        if (isHuman) return Drawable::Blue;
        return Drawable::Red;

    case MinimapMode::Normal:
        if (isGaia) return Drawable::Color(128, 192, 128);
        return playerMinimapColor(unit->playerId());

    case MinimapMode::Economic:
        if (isGaia) {
            // Gaia resources get distinct colors
            const int cls = unit->data()->Class;
            if (cls == genie::Unit::Tree || cls == genie::Unit::TreeStump) {
                return Drawable::Color(0, 160, 0);     // trees -- green
            }
            if (unit->data()->MinimapColor == 1) {     // gold ore
                return Drawable::Color(255, 215, 0);    // gold -- yellow
            }
            if (unit->data()->MinimapColor == 2) {     // stone ore
                return Drawable::Color(180, 180, 180);  // stone -- grey
            }
            if (cls == genie::Unit::BerryBush) {
                return Drawable::Color(220, 50, 50);    // berries -- red
            }
            if (cls == genie::Unit::Livestock || cls == genie::Unit::PreyAnimal ||
                cls == genie::Unit::PredatorAnimal) {
                return Drawable::Color(210, 180, 140);  // animals -- tan
            }
            return Drawable::Color(128, 192, 128);      // other gaia
        }
        if (!isHuman) return Drawable::Red;
        // Own units: color by carried resource
        if (unit->resources[genie::ResourceType::WoodStorage]  > 0) return Drawable::Color(0, 192, 0);
        if (unit->resources[genie::ResourceType::FoodStorage]  > 0) return Drawable::Color(255, 200, 0);
        if (unit->resources[genie::ResourceType::GoldStorage]  > 0) return Drawable::Color(255, 215, 0);
        if (unit->resources[genie::ResourceType::StoneStorage] > 0) return Drawable::Color(180, 180, 180);
        if (unit->data()->Class == genie::Unit::Civilian) {
            return Drawable::Color(255, 255, 100); // idle villager
        }
        return Drawable::Blue;
    }
}

static const char *modeString(Minimap::MinimapMode mode)
{
    switch (mode) {
    case Minimap::MinimapMode::Normal:     return "Normal";
    case Minimap::MinimapMode::Economic:   return "Economic";
    case Minimap::MinimapMode::Diplomatic: return "Diplomatic";
    }
    return "";
}

void Minimap::cycleMode()
{
    switch (m_mode) {
    case MinimapMode::Diplomatic: m_mode = MinimapMode::Economic;   break;
    case MinimapMode::Economic:   m_mode = MinimapMode::Normal;     break;
    case MinimapMode::Normal:     m_mode = MinimapMode::Diplomatic; break;
    }
    m_unitsUpdated = true;
    m_terrainUpdated = true;

    if (m_modeLabel) {
        m_modeLabel->string = modeString(m_mode);
    }
}


bool Minimap::init()
{
    m_terrainTexture = m_renderTarget->createTextureTarget(m_rect.size());
    DBG << "creating texture target with size" << m_rect.size();
    m_terrainUpdated = true;

    m_modeLabel = m_renderTarget->createText(Drawable::Text::Plain);
    m_modeLabel->pointSize = 10;
    m_modeLabel->color = Drawable::White;
    m_modeLabel->outlineColor = Drawable::Black;
    m_modeLabel->string = modeString(m_mode);

    return m_terrainTexture != nullptr;
}

bool Minimap::handleEvent(input::Event event)
{
    ScreenPos pos;
    if (event.type == input::Event::MouseButtonPressed || event.type == input::Event::MouseButtonReleased) {
        pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
    } else if (event.type == input::Event::MouseMoved && m_mousePressed) {
        pos = ScreenPos(event.mouseMove.x, event.mouseMove.y);
    } else {
        return false;
    }

    if (event.type == input::Event::MouseButtonPressed && m_rect.contains(pos)) {
        m_mousePressed = true;
    } else if (event.type == input::Event::MouseButtonReleased && m_mousePressed) {
        m_mousePressed = false;
        return true;
    }

    if (!m_rect.contains(pos)) {
        return m_mousePressed;
    }

    // normalize to 0,0
    pos.x -= m_rect.x;
    pos.y -= m_rect.y;

    pos.y = m_rect.height/2 - pos.y; // from the center

    const MapRect mapDimensions(0, 0, m_map->columnCount() * Constants::TILE_SIZE, m_map->rowCount() * Constants::TILE_SIZE);
    const ScreenRect fullBoundingRect = mapDimensions.boundingScreenRect();
    pos.x = fullBoundingRect.width * pos.x / m_rect.width;
    pos.y = fullBoundingRect.height * pos.y / m_rect.height;

    const MapPos mapPos = mapDimensions.bounded(pos.toMap());
    m_renderTarget->camera()->setTargetPosition(mapPos);

    return true;
}

bool Minimap::update(Time /*time*/)
{
    if (IS_UNLIKELY(!m_visibilityMap)) {
        WARN << "no visibility map set";
        return false;
    }

    if (m_lastCameraPos != m_renderTarget->camera()->m_target) {
        updateCamera();
        m_lastCameraPos = m_renderTarget->camera()->m_target;
    }

    const bool rectUpdated = updateRect(m_renderTarget->getSize());

    if (!m_map || (!m_unitsUpdated && !m_terrainUpdated && !rectUpdated)) {
        return false;
    }

    const MapRect mapDimensions(0, 0, m_map->columnCount(), m_map->rowCount());

    if (m_terrainUpdated || rectUpdated) {
        DBG << "redrawing terrain";
        if (!m_terrainTexture ||  m_terrainTexture->getSize() != m_rect.size()) {
            DBG << "recreating terrain";
            m_terrainTexture = m_renderTarget->createTextureTarget(m_rect.size());
        }

        m_terrainTexture->clear(Drawable::Transparent);

        const float scaleX = m_rect.boundingMapRect().width / mapDimensions.width / 2;
        const float scaleY = m_rect.boundingMapRect().height / mapDimensions.height / 2;

        Drawable::Circle background;
        background.aspectRatio = m_rect.height / m_rect.width;
        background.radius = std::floor(m_rect.width / 2);
        background.pointCount = 4;
        background.fillColor = Drawable::Black;
        background.filled = true;
        background.borderSize = 0;
        m_terrainTexture->draw(background);

        Drawable::Circle tileShape;
        tileShape.aspectRatio =  m_rect.height / m_rect.width;
        tileShape.radius = scaleY;
        tileShape.filled = true;
        tileShape.pointCount = 4;
        tileShape.borderSize = 0;
        const ScreenPos center(m_rect.width/2, m_rect.height/2);

        const std::vector<genie::Color> &colors = AssetManager::Inst()->getPalette(50500).getColors();
        for (int col = 0; col < m_map->columnCount(); col++) {
            for (int row = 0; row < m_map->rowCount(); row++) {
                const VisibilityMap::Visibility visibility = m_visibilityMap->visibilityAt(col, row);
                if (visibility == VisibilityMap::Unexplored) {
                    continue;
                }

                const MapTile &tile = m_map->getTileAt(col, row);
                const genie::Terrain &terrain = DataManager::Inst().getTerrain(tile.terrainId);
                const genie::Color &color = colors[terrain.Colors[0]];

                // Economic mode terrain tints
                const bool isWater = (tile.terrainId == 1 || tile.terrainId == 2 || tile.terrainId == 3 ||
                                      tile.terrainId == 4 || tile.terrainId == 22 || tile.terrainId == 26);
                const bool isFarm = (tile.terrainId == 7);

                if (m_mode == MinimapMode::Economic && isWater) {
                    // Brighter blue for water in economic mode
                    if (visibility == VisibilityMap::Explored) {
                        tileShape.fillColor = Drawable::Color(30, 60, 120);
                    } else {
                        tileShape.fillColor = Drawable::Color(60, 120, 240);
                    }
                } else if (m_mode == MinimapMode::Economic && isFarm) {
                    // Yellow for farmland in economic mode
                    if (visibility == VisibilityMap::Explored) {
                        tileShape.fillColor = Drawable::Color(100, 100, 0);
                    } else {
                        tileShape.fillColor = Drawable::Color(200, 200, 0);
                    }
                } else if (visibility == VisibilityMap::Explored) {
                    tileShape.fillColor = Drawable::Color(color.r/2, color.g/2, color.b/2);
                } else {
                    tileShape.fillColor = Drawable::Color(color.r, color.g, color.b);
                }

                // WTF TODO FIXME why the fuck is flipping row and col the correct here..
                const ScreenPos pos = MapPos(row * scaleX, col * scaleY).toScreen();
                tileShape.center = ScreenPos(pos.x, pos.y + center.y - scaleY / 2);
                m_terrainTexture->draw(tileShape);

            }
        }

        m_terrainTexture->display();

        m_terrainUpdated = false;
    }

    if ((m_unitsUpdated || rectUpdated) && m_unitManager) {
        TIME_THIS;

        if (!m_unitsTexture || m_unitsTexture->getSize() != m_rect.size()) {
            m_unitsTexture = m_renderTarget->createTextureTarget(m_rect.size());
        }
        m_unitsTexture->clear(Drawable::Transparent);


        const float scaleX = m_rect.boundingMapRect().width / mapDimensions.width / 2;
        const float scaleY = m_rect.boundingMapRect().height / mapDimensions.height / 2;

        const ScreenPos center(m_rect.width/2, m_rect.height/2);

        Drawable::Circle diamondSprite;
        diamondSprite.pointCount = 4;
        diamondSprite.radius = scaleY;
        diamondSprite.aspectRatio = m_rect.height / m_rect.width;
        diamondSprite.filled = true;
        diamondSprite.borderSize = 0;

        Drawable::Rect rectangleSprite;
        rectangleSprite.filled = true;
        rectangleSprite.borderSize = 0;

        const std::vector<genie::Color> &colors = AssetManager::Inst()->getPalette(50500).getColors();

        for (const Unit::Ptr &unit : m_unitManager->units()) {
            const VisibilityMap::Visibility visibility = m_visibilityMap->visibilityAt(unit->position());
            if (visibility == VisibilityMap::Unexplored) {
                continue;
            }
            if (visibility == VisibilityMap::Explored && unit->playerId() != UnitManager::GaiaID) {
                continue;
            }

            const genie::Unit::MinimapModes mode = genie::Unit::MinimapModes(unit->data()->MinimapMode);
            if (mode == genie::Unit::MinimapInvisible) {
                continue;
            }
            if (mode == genie::Unit::MinimapFlying) {
                continue;
            }

            if (mode != genie::Unit::MinimapUnit && mode != genie::Unit::MinimapBuilding &&
                mode != genie::Unit::MinimapLargeTerrain && mode != genie::Unit::MinimapLargeTerrain2) {
                DBG << "Unhandled minimap mode" << int(mode) << unit->data()->MinimapColor;
                continue;
            }

            const MapPos mapPos = unit->position();
            ScreenPos pos = MapPos(mapPos.y / Constants::TILE_SIZE, mapPos.x / Constants::TILE_SIZE - 1).toScreen();
            float size = std::max(unit->data()->OutlineSize.x * scaleX * 2, 2.f);
            pos.x = pos.x * scaleX - size/2;
            pos.y = pos.y * scaleY + center.y - size/2;

            // WARN: according to genieutils this is the inverted of what the game officially does,
            // but squares on the minimap look soooo ugly
            if (mode == genie::Unit::MinimapBuilding) {
                rectangleSprite.rect = ScreenRect(pos, Size(size, size));
                rectangleSprite.fillColor = unitColor(unit);
                m_unitsTexture->draw(rectangleSprite);
            } else if (mode == genie::Unit::MinimapUnit) {
                diamondSprite.fillColor = unitColor(unit);
                diamondSprite.center = pos;
                diamondSprite.radius = size;
                m_unitsTexture->draw(diamondSprite);
            } else if (mode == genie::Unit::MinimapLargeTerrain || mode == genie::Unit::MinimapLargeTerrain2) {
                rectangleSprite.rect = ScreenRect(pos, Size(size, size));
                const genie::Color &color = colors[unit->data()->MinimapColor];
                rectangleSprite.fillColor = Drawable::Color(color.r, color.g, color.b);
                m_unitsTexture->draw(rectangleSprite);
            }
        }
        m_unitsTexture->display();
        m_unitsUpdated = false;
    }
    return true;
}

void Minimap::draw()
{
    m_renderTarget->draw(m_terrainTexture, m_rect.topLeft());
    m_renderTarget->draw(m_unitsTexture, m_rect.topLeft());

    // Draw mode label above minimap
    if (m_modeLabel) {
        m_modeLabel->position = ScreenPos(m_rect.x + 2, m_rect.y - 14);
        m_renderTarget->draw(m_modeLabel);
    }

    if (m_rect.isEmpty()) {
        return;
    }

    const ScreenRect cameraRect = m_cameraRect.intersected(m_rect);
    if (cameraRect.isEmpty()) {
        return;
    }

    Drawable::Rect rect;
    rect.rect = ScreenRect(ScreenPos(cameraRect.x, cameraRect.y + 1), Size(cameraRect.size()));
    rect.fillColor = Drawable::Transparent;
    rect.filled = true;
    rect.borderSize = 1;

    rect.borderColor = Drawable::Black;
    m_renderTarget->draw(rect);

    rect.rect.setTopLeft(cameraRect.topLeft());
    rect.borderColor = Drawable::White;
    m_renderTarget->draw(rect);
}
