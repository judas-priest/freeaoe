#include "FormationHelper.h"
#include "core/Constants.h"
#include <cmath>

std::vector<MapPos> FormationHelper::computePositions(
    const std::vector<Unit::Ptr> &units,
    const MapPos &targetCenter,
    Unit::Formation formation,
    float facingAngle)
{
    const size_t count = units.size();
    std::vector<MapPos> positions(count, targetCenter);
    if (count <= 1) return positions;

    const float spacing = Constants::TILE_SIZE * 1.5f;
    const float cosA = std::cos(facingAngle);
    const float sinA = std::sin(facingAngle);
    const float perpX = -sinA;
    const float perpY = cosA;

    switch (formation) {
    case Unit::Formation::Line: {
        float totalWidth = (count - 1) * spacing;
        float startOffset = -totalWidth / 2.f;
        for (size_t i = 0; i < count; ++i) {
            float offset = startOffset + i * spacing;
            positions[i].x = targetCenter.x + perpX * offset;
            positions[i].y = targetCenter.y + perpY * offset;
        }
        break;
    }
    case Unit::Formation::Box: {
        int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
        int rows = static_cast<int>(std::ceil(static_cast<float>(count) / cols));
        float colStart = -(cols - 1) * spacing / 2.f;
        float rowStart = -(rows - 1) * spacing / 2.f;
        for (size_t i = 0; i < count; ++i) {
            int col = i % cols;
            int row = i / cols;
            float perpOffset = colStart + col * spacing;
            float fwdOffset = rowStart + row * spacing;
            positions[i].x = targetCenter.x + perpX * perpOffset + cosA * fwdOffset;
            positions[i].y = targetCenter.y + perpY * perpOffset + sinA * fwdOffset;
        }
        break;
    }
    case Unit::Formation::Flank: {
        positions[0] = targetCenter;
        for (size_t i = 1; i < count; ++i) {
            int side = (i % 2 == 1) ? 1 : -1;
            int rank = static_cast<int>((i + 1) / 2);
            float perpOffset = side * rank * spacing;
            float fwdOffset = -rank * spacing * 0.7f;
            positions[i].x = targetCenter.x + perpX * perpOffset + cosA * fwdOffset;
            positions[i].y = targetCenter.y + perpY * perpOffset + sinA * fwdOffset;
        }
        break;
    }
    case Unit::Formation::SpreadOut: {
        float radius = count * spacing / (2.f * static_cast<float>(M_PI));
        radius = std::max(radius, spacing);
        for (size_t i = 0; i < count; ++i) {
            float angle = (2.f * static_cast<float>(M_PI) * i) / count;
            positions[i].x = targetCenter.x + std::cos(angle) * radius;
            positions[i].y = targetCenter.y + std::sin(angle) * radius;
        }
        break;
    }
    }

    return positions;
}
