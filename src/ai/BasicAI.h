#pragma once

#include "core/Types.h"
#include <memory>

struct AiPlayer;
class UnitManager;

// Simple hardcoded AI that trains villagers, builds houses, and makes military
class BasicAI
{
public:
    BasicAI(AiPlayer *player, UnitManager *unitManager);

    void update(Time time);

private:
    AiPlayer *m_player;
    UnitManager *m_unitManager;
    Time m_lastUpdate = 0;

    void trainVillagers();
    void buildHouses();
    void trainMilitary();
    void researchTechs();

    int countUnitsOfType(int unitId) const;
    int countBuildingsOfType(int buildingId) const;
};
