#include "WorldBlock.h"

#include "World.h"
#include "Math/Random.h"
using namespace Math;
using namespace World;

void WorldBlock::InitBlocks()
{
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                if (RandomNumberGenerator().NextFloat() < 0.3)
                {
                    continue;
                }
                int type = RandomNumberGenerator().NextInt(0,2);
                Vector3 pointPos = originPoint + Vector3(x + 0.5f, y + 0.5f, z + 0.5f) * UnitBlockSize;
                pointPos = Vector3(pointPos.GetX(), pointPos.GetZ(), pointPos.GetY());
                blocks[x][y][z] = Block(pointPos, BlockResourceManager::BlockType(type), UnitBlockRadius);
                blocks[x][y][z].model.Resize(World::UnitBlockRadius);
                blocks[x][y][z].model.Translate(pointPos);
            }
        }
    }
}

void WorldBlock::Update(GraphicsContext& gfxContext, float deltaTime)
{
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z =0; z < worldBlockDepth; z++)
            {
                blocks[x][y][z].Update(gfxContext, deltaTime);
            }
            
        }
    }
}

void WorldBlock::Render(Renderer::MeshSorter& sorter)
{
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z =0; z < worldBlockDepth; z++)
            {
                blocks[x][y][z].Render(sorter);
            }
            
        }
    }
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
