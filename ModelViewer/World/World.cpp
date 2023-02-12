﻿#include "World.h"

#include <cmath>

namespace World
{
    uint16_t UnitBlockSize = 30;
    float UnitBlockRadius = sqrt(std::pow(UnitBlockSize, 2)+std::pow(UnitBlockSize,2) + std::pow(UnitBlockSize,2)) /2 *1;
}
