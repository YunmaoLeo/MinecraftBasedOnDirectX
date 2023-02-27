#pragma once
#include "SimplexNoise.h"
#include "../Blocks/BlockResourceManager.h"

using namespace BlockResourceManager;

namespace WorldGenerator
{
    constexpr float HUMIDITY_DRY = 0.3;
    constexpr float HUMIDITY_WET = 1;
    constexpr float TEMP_COLD = 0.3;
    constexpr float TEMP_WARM = 0.7;
    constexpr float TEMP_HOT = 1;
    constexpr int WORLD_DEPTH = 128;
    constexpr int SEA_HEIGHT = 30;
    constexpr int SURFACE_HEIGHT = 20;
    constexpr float COOR_STEP = 0.0006;
    extern SimplexNoise continent;
    extern SimplexNoise erosion;
    extern SimplexNoise peaksValleys;
    extern SimplexNoise temperature;
    extern SimplexNoise humidity;



    struct Biomes
    {
        BlockType SurfaceBlock;
        BlockType ShallowSurfaceBlock;
        int ShallowSurfaceDepth;
        float PlantDensity;
        int TreeHeight;
        int TreeWidth;

        Biomes(BlockType surface, BlockType shallow, int depth, float density, int treeHeight, int treeWidth)
            : SurfaceBlock(surface),
              ShallowSurfaceBlock(shallow),
              PlantDensity(density),
              ShallowSurfaceDepth(depth),
                TreeHeight(treeHeight),
        TreeWidth(treeWidth)
        
        {
        }

        Biomes()
        {
        }
    };

    int getRealHeightAndBiomes(float x, float y, Biomes& biomes);

    BlockType getBlockType(float xCoor, float yCoor, int height, Biomes biomes, int z);


    Biomes getBiomes(float x, float y);

    extern Biomes BarrenIceField;
    extern Biomes InlandForest;
    extern Biomes Desert;
    extern Biomes FlourishIceField;
    extern Biomes Forest;
    extern Biomes RainForest;
};
