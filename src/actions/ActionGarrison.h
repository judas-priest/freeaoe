#pragma once

#include "IAction.h"
struct Building;

class ActionGarrison : public IAction
{
public:
    ActionGarrison(const std::shared_ptr<Unit> &unit, const Task &task);

    genie::ActionType taskType() const override {
        return genie::ActionType::Garrison;
    }

    UpdateResult update(Time time) override;

private:
    std::weak_ptr<Building> m_buildingTarget;
    std::weak_ptr<Unit> m_unitTarget; // For non-building garrison (transport ships, rams)
    bool m_isUnitTarget = false;
};


