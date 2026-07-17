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
    enum class Strategy {
        Balanced,
        Rush,     // Fast military, early aggression
        Boom,     // Max villagers, fast age-up, delayed military
        Turtle,   // Walls, towers, defensive
    };

    BasicAI(AiPlayer *player, UnitManager *unitManager);

    void update(Time time);
    void applyParams(const ai::DifficultyParams &p) { m_params = p; }

private:
    AiPlayer *m_player;
    UnitManager *m_unitManager;
    Time m_lastUpdate = 0;
    Time m_lastDiplomacyUpdate = 0;
    ai::DifficultyParams m_params;

    Strategy m_strategy = Strategy::Balanced;
    bool m_strategyChosen = false;
    void chooseStrategy();

    void scoutMap();
    void defendAgainstThreats();
    void retreatInjuredUnits();
    void garrisonVillagersUnderAttack();
    void ungarrisonWhenSafe();
    void trainVillagers();
    void buildHouses();
    void assignIdleVillagers();
    void buildDropOffSites();
    void buildDefenses();
    void researchLoom();
    void advanceAge();
    void attackWithArmy();
    void trainMilitary();
    void useMonksOffensively();
    void researchTechs();
    void updateDiplomacy();
    void useMarket();
    void buildNaval();
    bool isWaterMap() const;
    void buildStructure(int buildingId, int woodCost);
    void buildStructureWithCost(int buildingId, int woodCost, int stoneCost);
    void trainFromBuilding(int buildingId, int unitId);
    bool isMilitaryUnit(int unitId) const;

    int countUnitsOfType(int unitId) const;
    int countBuildingsOfType(int buildingId) const;
};
