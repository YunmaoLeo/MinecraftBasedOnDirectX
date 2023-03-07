#include "WorldGenerator.h"

#include <iostream>
#include <ostream>

#include "World.h"

namespace WorldGenerator
{
    SimplexNoise continent(0.05, 1, 2, 0.5);
    SimplexNoise erosion(0.02, 1, 2, 0.5);
    SimplexNoise peaksValleys(0.6, 1, 2, 0.5);
    SimplexNoise temperature(0.01, 1, 2, 0.5);
    SimplexNoise humidity(0.01, 1, 2, 0.5);
    SimplexNoise caves(0.8, 1, 2, 0.5);

    Biomes BarrenIceField(GrassSnow, Dirt, 7, 0.01, 20, 1);
    Biomes InlandForest(GrassWilt, Dirt, 7, 0.01, 15, 1);
    Biomes InlandPlain(GrassWilt, Dirt, 7, 0.05, 0,0);
    Biomes Desert(Sand, Sand, 12, 0, 0, 0);
    Biomes FlourishIceField(GrassSnow, Dirt, 9, 0.05, 15, 1);
    Biomes Forest(Grass, Dirt, 8, 0.05, 6, 1);
    Biomes RainForest(Grass, Dirt, 12, 0.10, 15, 2);
    Biomes Savanna(Grass, Dirt, 12, 0.1, 0, 0);
    
    int ANOTHER_HEIGHT = SEA_HEIGHT + 12;
    std::map<float, float> ContinentalnessNodes = {
        {-1.0, ANOTHER_HEIGHT},
        {-0.4, ANOTHER_HEIGHT},
        {-0.5, ANOTHER_HEIGHT + 15},
        {0.0, ANOTHER_HEIGHT + 15},
        {0.1, ANOTHER_HEIGHT + 25},
        {0.2, ANOTHER_HEIGHT + 40},
        {0.5, ANOTHER_HEIGHT + 45},
        {1.0, ANOTHER_HEIGHT + 50},
    };

    std::map<float, float> ErosionNodes = {
        {-1.0, 25},
        {-0.7, 10},
        {-0.6, 15},
        {-0.2, 12},
        {0.2, -10},
        {0.4,-20},
        {0.6,-20},
        {0.65,-10},
        {0.7,-10},
        {0.75, -20},
        {1.0, -25}
    };

    std::map<float, float> PeakValleysNodes = {
        {-1.0, -5},
        {-0.8, -3},
        {-0.5, -1},
        {0.0, 1.0},
        {0.5, 8},
        {0.7, 12},
        {1.0, 10},
    };
    
    BlockType getBlockType(float xCoor, float yCoor, int height, Biomes biomes, int z)
    {
        // return air blocks
        if (height <= SEA_HEIGHT && z> SURFACE_HEIGHT && z == SEA_HEIGHT)
        {
            return Water;
        }
        if (z > height)
        {
            return Air;
        }

        //dig out caves
        // if (height > SEA_HEIGHT && z >= height - 35 && z <= height -10 && z>=SURFACE_HEIGHT)
        // {
        //     float zCoor = (z + 0.5f) * World::UnitBlockSize * 1.001 * COOR_STEP;
        //     float isCave = caves.fractal(5, xCoor, yCoor, zCoor);
        //     if (abs(isCave) < 0.1)
        //     {
        //         return BlocksCount;
        //     }
        // }

        // surfaceBlock
        if (z == height)
        {
            return biomes.SurfaceBlock;
        }
        if (z <= height - 1 && z >= height - 1 - biomes.ShallowSurfaceDepth)
        {
            return biomes.ShallowSurfaceBlock;
        }

        return Stone;
    }

    float getSplineValue(float value, std::map<float, float> nodes)
    {
        float min = 0;
        float max = 0;
        float start = 0;
        float end = 0;
        for (auto node : nodes)
        {
            if (node.first < value)
            {
                start = node.first;
                min = node.second;
            }
            else
            {
                end = node.first;
                max = node.second;
                break;
            }
        }
        if (max == min)
        {
            return max;
        }

        float k = (max - min) / (end - start);
        float b = max - end * k;

        return value * k + b;
    }


    int getRealHeightAndBiomes(float x, float y, Biomes& biomes)
    {
        float cont = continent.fractal(4, x, y);
        float ero = erosion.fractal(5, x, y);
        float pea = peaksValleys.fractal(4, x, y);

        float height;
        // float height = SURFACE_HEIGHT + 5 + (WORLD_DEPTH - SURFACE_HEIGHT) * 0.3 * (cont + 1) / 2;
        height = getSplineValue(cont, ContinentalnessNodes);
        height += getSplineValue(ero, ErosionNodes);
        height += getSplineValue(pea, PeakValleysNodes);

        if (height >= WORLD_DEPTH * 0.95) height = WORLD_DEPTH * 0.95;
        if (height < 0) height = 0;

        float h = humidity.fractal(4, x, y);
        h = (h + 1) / 2;
        float t = temperature.fractal(4, x, y);
        t = (t + 1) / 2;
        Biomes result;

        if (cont < 0.1)
        {
            // coastline
            if (ero < 0.0)
            {
                // coastline hills
                if (t < TEMP_COLD)
                {
                    result = FlourishIceField;
                }
                else if (t<TEMP_WARM)
                {
                    result = Savanna;
                }
                else if (t<TEMP_HOT)
                {
                    if (h > HUMIDITY_DRY) result  = RainForest;
                    else result = Savanna;
                }
            }
            else
            {
                result = Forest;
            }

        }
        else
        {
            // far inland
            if (ero < 0.0)
            {
                //inland mountains;
                if (t < TEMP_COLD) result = BarrenIceField;
                else result = InlandForest;
            }
            else
            {
                if (t > TEMP_WARM) result = Desert;
                else result = InlandPlain;
            }
        }
        
        result.ShallowSurfaceDepth = result.ShallowSurfaceDepth * (1 + (random.NextInt(40) - 10) / 100);
        biomes = result;

        return height;
    }
}
