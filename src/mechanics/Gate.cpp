#include "Gate.h"
#include "Map.h"
#include "Player.h"
#include "render/GraphicRender.h"
#include "core/Constants.h"
#include "core/Logger.h"

Gate::Gate(const genie::Unit &data_, const std::shared_ptr<Player> &player, UnitManager &unitManager)
    : Building(data_, player, unitManager)
{
}

bool Gate::isPassableFor(int queryPlayerId) const noexcept
{
    if (isLocked) return false;
    if (!isOpen)  return false;

    Player::Ptr owner = player().lock();
    if (!owner) return false;

    if (owner->playerId == queryPlayerId) return true;
    return owner->isAllied(static_cast<uint8_t>(queryPlayerId));
}

void Gate::setOpen(bool open) noexcept
{
    if (isOpen == open) return;
    isOpen = open;

    int targetSpriteId = open
        ? data()->StandingGraphic.first
        : data()->Building.ConstructionGraphicID;

    if (targetSpriteId >= 0) {
        renderer().setSprite(targetSpriteId);
    }
}

bool Gate::update(Time time) noexcept
{
    bool ret = Building::update(time);

    if (isLocked) {
        setOpen(false);
        return ret;
    }

    MapPtr m = map();
    if (!m) return ret;

    Player::Ptr owner = player().lock();
    if (!owner) return ret;

    const MapPos centre = position();
    const int tileX = static_cast<int>(centre.x / Constants::TILE_SIZE_F);
    const int tileY = static_cast<int>(centre.y / Constants::TILE_SIZE_F);
    const int halfW = std::max(1, static_cast<int>(std::ceil(data()->Size.x)));
    const int halfH = std::max(1, static_cast<int>(std::ceil(data()->Size.y)));

    bool friendlyInFootprint = false;
    for (int dx = -halfW; dx <= halfW && !friendlyInFootprint; ++dx) {
        for (int dy = -halfH; dy <= halfH && !friendlyInFootprint; ++dy) {
            const int col = tileX + dx;
            const int row = tileY + dy;
            if (col < 0 || row < 0 || col >= m->columnCount() || row >= m->rowCount()) continue;

            for (const std::weak_ptr<Entity> &e : m->entitiesAt(col, row)) {
                Unit::Ptr u = Unit::fromEntity(e);
                if (!u || u.get() == this) continue;
                if (u->data()->Speed == 0) continue; // skip buildings/resources

                const int uid = u->playerId();
                if (uid == owner->playerId || owner->isAllied(static_cast<uint8_t>(uid))) {
                    friendlyInFootprint = true;
                    break;
                }
            }
        }
    }

    setOpen(friendlyInFootprint);
    return ret;
}
