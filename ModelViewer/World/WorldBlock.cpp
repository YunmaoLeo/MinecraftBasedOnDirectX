#include "WorldBlock.h"

#include "World.h"
#include "Math/Random.h"
using namespace Math;
using namespace World;

void WorldBlock::InitBlocks()
{
    RandomNumberGenerator generator;
    generator.SetSeed(1);
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                if (generator.NextFloat() < 0.3)
                {
                    continue;
                }
                int type = generator.NextInt(0, 1);
                Vector3 pointPos = originPoint + Vector3(x + 0.5f, y + 0.5f, z + 0.5f) * UnitBlockSize;
                pointPos = Vector3(pointPos.GetX(), pointPos.GetZ(), pointPos.GetY());
                blocks[x][y][z] = Block(pointPos, BlockResourceManager::BlockType(type), UnitBlockRadius);
                blocks[x][y][z].model.Resize(World::UnitBlockRadius);
                blocks[x][y][z].model.Translate(pointPos);
            }
        }
    }

    SearchBlocksAdjacent2OuterAir();
}

void WorldBlock::Update(GraphicsContext& gfxContext, float deltaTime)
{
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                blocks[x][y][z].Update(gfxContext, deltaTime);
            }
        }
    }
}

void WorldBlock::SearchBlocksAdjacent2OuterAir()
{
    std::vector<std::vector<std::vector<int>>> blocksStatus(worldBlockSize,
                                                            std::vector<std::vector<int>>(worldBlockSize,
                                                                std::vector<int>(worldBlockDepth)));

    // Assign outermost blocks as adjacent to outer air.
    // Outermost blocks has a same attribute, one of x,y equals 0 or worldBlockSize-1
    //                                  or z equals 0 or worldBlockDepth-1;
    for (int y = 0; y < worldBlockSize; y++)
    {
        for (int z = 0; z < worldBlockDepth; z++)
        {
            blocks[0][y][z].adjacent2OuterAir = true;
            blocks[worldBlockSize - 1][y][z].adjacent2OuterAir = true;
        }
    }

    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int z = 0; z < worldBlockDepth; z++)
        {
             blocks[x][0][z].adjacent2OuterAir = true;
            blocks[x][worldBlockSize - 1][z].adjacent2OuterAir = true;
        }
    }

    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            blocks[x][y][0].adjacent2OuterAir = true;
             blocks[x][y][worldBlockDepth - 1].adjacent2OuterAir = true;
        }
    }
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                if (blocks[x][y][z].IsNull() && blocks[x][y][z].adjacent2OuterAir)
                {
                    SpreadAdjacent2OuterAir(x, y, z, blocksStatus);
                }
            }
        }
    }
}

void WorldBlock::Render(Renderer::MeshSorter& sorter, const Camera& camera)
{
    int count = 0;
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                Block& block = blocks[x][y][z];
                
                if (!block.IsNull()
                    && block.adjacent2OuterAir
                    && camera.GetWorldSpaceFrustum().IntersectSphere(block.model.GetBoundingSphere()))
                {
                    count ++;
                    block.Render(sorter);
                }
            }
        }
    }
    std::cout << "blocksRenderCount: " << count << std::endl;
}

void WorldBlock::CleanUp()
{
    for (auto blocksX : blocks)
    {
        for (auto blocksY : blocksX)
        {
            for (auto block : blocksY)
            {
                block.CleanUp();
            }
        }
    }
}

void WorldBlock::SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>>& blockStatus)
{
    // out of boundary
    if (x < 0 || x > worldBlockSize - 1 || y < 0 || y > worldBlockSize - 1 || z < 0 || z > worldBlockDepth - 1)
    {
        return;
    }

    blocks[x][y][z].adjacent2OuterAir = true;

    if (blockStatus[x][y][z])
    {
        return;
    }

    blockStatus[x][y][z] = 1;

    if (blocks[x][y][z].IsNull() || blocks[x][y][z].isTransparent())
    {
        SpreadAdjacent2OuterAir(x + 1, y, z, blockStatus);
        SpreadAdjacent2OuterAir(x - 1, y, z, blockStatus);
        SpreadAdjacent2OuterAir(x, y + 1, z, blockStatus);
        SpreadAdjacent2OuterAir(x, y - 1, z, blockStatus);
        SpreadAdjacent2OuterAir(x, y, z + 1, blockStatus);
        SpreadAdjacent2OuterAir(x, y, z - 1, blockStatus);
    }
}
