#pragma once
#include "Building.h"

class Gate : public Building {
public:
    using Ptr = std::shared_ptr<Gate>;

    Gate(const genie::Unit &data_, const std::shared_ptr<Player> &player, UnitManager &unitManager);

    bool isLocked = false;
    bool isOpen   = false;

    bool isPassableFor(int playerId) const noexcept;
    void setOpen(bool open) noexcept;
    bool update(Time time) noexcept override;

    static Ptr fromUnit(const Unit::Ptr &unit) noexcept {
        return std::dynamic_pointer_cast<Gate>(unit);
    }
};
