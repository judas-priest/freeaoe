#pragma once

#include "core/Types.h"
#include "ai/DifficultyParams.h"
#include <memory>

struct AiPlayer;
class UnitManager;

// Simple hardcoded AI that trains villagers, builds houses, and makes military
class BasicAI
{
public:
    BasicAI(AiPlayer *player, UnitManager *unitManager);

    void update(Time time);
    void applyParams(const ai::DifficultyParams &p) { m_params = p; }

private:
    AiPlayer *m_player;
    UnitManager *m_unitManager;
    Time m_lastUpdate = 0;
    ai::DifficultyParams m_params;

    void scoutMap();
    void defendAgainstThreats();
    void retreatInjuredUnits();
    void trainVillagers();
    void buildHouses();
    void assignIdleVillagers();
    void buildDropOffSites();
    void researchLoom();
    void advanceAge();
    void attackWithArmy();
    void trainMilitary();
    void researchTechs();
    void buildStructure(int buildingId, int woodCost);
    void trainFromBuilding(int buildingId, int unitId);
    bool isMilitaryUnit(int unitId) const;

    int countUnitsOfType(int unitId) const;
    int countBuildingsOfType(int buildingId) const;
};
