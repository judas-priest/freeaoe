#include "DifficultyParams.h"

namespace ai {

DifficultyParams DifficultyParams::forLevel(DifficultyLevel level)
{
    switch (level) {
    case DifficultyLevel::Easiest:  return { 10, 8,  15, 8000, 1.0f,  false, false };
    case DifficultyLevel::Easy:     return { 15, 15, 12, 6000, 0.33f, false, false };
    default:
    case DifficultyLevel::Moderate: return { 20, 30,  8, 5000, 0.0f,  true,  true  };
    case DifficultyLevel::Hard:     return { 35, 50,  6, 4000, 0.0f,  true,  true  };
    case DifficultyLevel::Hardest:  return { 75, 80,  4, 3000, 0.0f,  true,  true  };
    }
}

} // namespace ai
