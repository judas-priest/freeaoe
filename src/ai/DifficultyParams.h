#pragma once
#include "ai/gen/enums.h"

namespace ai {

struct DifficultyParams {
    int   villagerCap;
    int   militaryCap;
    int   attackThreshold;
    int   updateIntervalMs;
    float gatherBonus;
    bool  researchTechs;
    bool  buildSiege;

    static DifficultyParams forLevel(DifficultyLevel level);
};

} // namespace ai
