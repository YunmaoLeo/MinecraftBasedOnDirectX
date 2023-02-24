#include "WorldGenerator.h"

#include <iostream>
#include <ostream>

#include "World.h"

namespace WorldGenerator
{
    SimplexNoise continent(0.08, 0.6, 2, 0.5);
    SimplexNoise erosion(0.15, 0.7, 2, 0.5);
    SimplexNoise peaksValleys(0.6, 1, 2, 0.5);
    SimplexNoise temperature(0.05, 1, 2, 0.5);
    SimplexNoise humidity(0.05, 1, 2, 0.5);
    SimplexNoise caves(0.25, 1, 2, 0.5);

    Biomes BarrenIceField(GrassSnow, Dirt, 7, 0.02);
    Biomes FarInland(GrassWilt, Dirt, 7, 0.02);
    Biomes Desert(Sand, Sand, 12, 0.02);
    Biomes FlourishIceField(GrassSnow, Dirt, 9, 0.1);
    Biomes Forest(Grass, Dirt, 8, 0.1);
    Biomes RainForest(Grass, Dirt, 12, 0.15);

    BlockType getBlockType(float xCoor, float yCoor, int height, Biomes biomes, int z)
    {
        // return air blocks
        if (z > height)
        {
            return BlocksCount;
        }

        // dig out caves
        // if (z >= SEA_HEIGHT - 25 && z <= SEA_HEIGHT - 2)
        // {
        //     float zCoor = (z + 0.5f) * World::UnitBlockSize * 1.001 * COOR_STEP;
        //     float isCave = caves.fractal(5, xCoor, yCoor, zCoor);
        //     if (isCave > 0 && isCave < 0.15)
        //     {
        //         return BlocksCount;
        //     }
        // }

        // surfaceBlock
        if (z == height)
        {
            return biomes.SurfaceBlock;
        }
        if (z<=height-1 && z>=height - 1 - biomes.ShallowSurfaceDepth)
        {
            return biomes.ShallowSurfaceBlock;
        }

        return Stone;
    }

    Biomes getBiomes(float x, float y)
    {
        float h = humidity.fractal(4,x,y);
        h = (h+1)/2;
        float t = temperature.fractal(4, x,y);
        t = (t+1)/2;
        Biomes result;
        if (h < HUMIDITY_DRY)
        {
            if (t < TEMP_COLD)
            {
                result = BarrenIceField;
            }
            else if (t < TEMP_WARM)
            {
                result = FarInland;
            }
            else
            {
                result = Desert;
            }
            result.plantDensity = result.plantDensity * (1 + h);
        }
        else
        {
            if (t < TEMP_COLD)
            {
                result = FlourishIceField;
            }
            else if (t < TEMP_WARM)
            {
                result = Forest;
            }
            else
            {
                result = RainForest;
            }
            result.plantDensity = result.plantDensity * (1 + (h / 7));
        }
        result.ShallowSurfaceDepth = result.ShallowSurfaceDepth * (1+(random.NextInt(20)-10)/100);

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
        // if (ero < 0)
        // {
        //     height -= ero * ero / 1 * (WORLD_DEPTH - SURFACE_HEIGHT) * 0.05;
        // }

        height += pea * (WORLD_DEPTH - SURFACE_HEIGHT) * 0.1;

        if (height >= WORLD_DEPTH * 0.95) height = WORLD_DEPTH * 0.95;
        if (height < 0) height = 0;

        return height;
    }
}
