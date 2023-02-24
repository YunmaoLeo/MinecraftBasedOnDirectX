#include "WorldGenerator.h"

#include <iostream>
#include <ostream>

#include "World.h"

namespace WorldGenerator
{
    SimplexNoise continent(0.02, 0.6, 2, 0.5);
    SimplexNoise erosion(0.15, 0.7, 2, 0.5);
    SimplexNoise peaksValleys(0.6, 1, 2, 0.5);
    SimplexNoise temperature(0.01, 1, 2, 0.5);
    SimplexNoise humidity(0.01, 1, 2, 0.5);
    SimplexNoise caves(0.25, 1, 2, 0.5);

    Biomes BarrenIceField(GrassSnow, Dirt, 7, 0.08);
    Biomes FarInland(GrassWilt, Dirt, 7, 0.1);
    Biomes Desert(Sand, Sand, 12, 0.1);
    Biomes FlourishIceField(GrassSnow, Dirt, 9, 0.3);
    Biomes Forest(Grass, Dirt, 8, 0.4);
    Biomes RainForest(Grass, Dirt, 12, 0.65);

    BlockType getBlockType(float xCoor, float yCoor, float height, Biomes biomes, int z)
    {
        if (z >= height && z > SEA_HEIGHT)
        {
            return BlocksCount;
        }

        if (z >= SEA_HEIGHT - 25 && z <= SEA_HEIGHT - 2)
        {
            float zCoor = (z + 0.5f) * World::UnitBlockSize * 1.001 * COOR_STEP;
            float isCave = caves.fractal(5, xCoor, yCoor, zCoor);
            if (isCave > 0 && isCave < 0.15)
            {
                return BlocksCount;
            }
        }

        return Stone;
    }

    Biomes getBiomes(float x, float y)
    {
        float humidity;
        float temp;
        Biomes result;
        if (humidity < HUMIDITY_DRY)
        {
            if (temp < TEMP_COLD)
            {
                result = BarrenIceField;
            }
            else if (temp < TEMP_WARM)
            {
                result = FarInland;
            }
            else
            {
                result = Desert;
            }
            result.plantDensity = result.plantDensity * (1 + humidity);
        }
        else
        {
            if (temp < TEMP_COLD)
            {
                result = FlourishIceField;
            }
            else if (temp < TEMP_WARM)
            {
                result = Forest;
            }
            else
            {
                result = RainForest;
            }
            result.plantDensity = result.plantDensity * (1 + (humidity / 3));
        }

        return result;
    }

    int getRealHeight(float x, float y)
    {
        double cont = continent.fractal(4, x, y);
        double ero = erosion.fractal(5, x, y);
        double pea = peaksValleys.fractal(4, x, y);

        float height = SURFACE_HEIGHT + 5 + (WORLD_DEPTH - SURFACE_HEIGHT) * 0.3 * (cont + 1) / 2;

        if (ero > 0)
        {
            height -= ero / 1 * (WORLD_DEPTH - SURFACE_HEIGHT) * 0.2;
        }
        if (ero < 0)
        {
            height -= ero * ero / 1 * (WORLD_DEPTH - SURFACE_HEIGHT) * 0.05;
        }

        height += pea * (WORLD_DEPTH - SURFACE_HEIGHT) * 0.1;

        if (height >= WORLD_DEPTH * 0.95) height = WORLD_DEPTH * 0.95;
        if (height < 0) height = 0;

        return height;
    }
}
