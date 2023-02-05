#include "World.h"

namespace World
{
    uint16_t UnitBlockSize = 100;
    float UnitBlockRadius = sqrt(std::pow(UnitBlockSize, 2)+std::pow(UnitBlockSize,2) + std::pow(UnitBlockSize,2)) /2 ;
}