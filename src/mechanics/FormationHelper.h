#pragma once

#include "core/Types.h"
#include "Unit.h"
#include <vector>
#include <memory>

struct FormationHelper {
    static std::vector<MapPos> computePositions(
        const std::vector<Unit::Ptr> &units,
        const MapPos &targetCenter,
        Unit::Formation formation,
        float facingAngle
    );
};
