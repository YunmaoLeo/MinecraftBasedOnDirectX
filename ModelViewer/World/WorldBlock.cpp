#include "WorldBlock.h"

#include "World.h"
#include "Math/Random.h"
using namespace Math;
using namespace World;

NumVar MaxOctreeDepth("Octree/Depth", 2, 0, 10, 1);
NumVar MaxOctreeNodeLength("Octree/Length", 2, 0, 20, 1);
BoolVar EnableOctree("Octree/EnableOctree", true);
BoolVar EnableContainTest("Octree/EnableContainTest", true);

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

void WorldBlock::RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                                     Renderer::MeshSorter& sorter, const Camera& camera)
{
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                Block& block = blocks[x][y][z];

                if (!block.IsNull()
                    && block.adjacent2OuterAir
                    && camera.GetWorldSpaceFrustum().IntersectBoundingBox(block.model.GetAxisAlignedBox()))
                {
                    count++;
                    block.Render(sorter);
                }
            }
        }
    }
}

void WorldBlock::RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                                     Renderer::MeshSorter& sorter, const Camera& camera)
{
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                Block& block = blocks[x][y][z];

                if (!block.IsNull()
                    && block.adjacent2OuterAir)
                {
                    count++;
                    block.Render(sorter);
                }
            }
        }
    }
}

void WorldBlock::OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
                                    Renderer::MeshSorter& sorter, const Camera& camera)
{
    //check bounding box intersection
    Block& block1 = blocks[minX][minY][minZ];
    Block& block2 = blocks[minX][maxY][minZ];
    Block& block3 = blocks[minX][minY][maxZ];
    Block& block4 = blocks[minX][maxY][maxZ];
    Block& block5 = blocks[maxX][minY][minZ];
    Block& block6 = blocks[maxX][maxY][minZ];
    Block& block7 = blocks[maxX][minY][maxZ];
    Block& block8 = blocks[maxX][maxY][maxZ];

    AxisAlignedBox box;
    box.AddBoundingBox(block1.model.GetAxisAlignedBox());
    box.AddBoundingBox(block2.model.GetAxisAlignedBox());
    box.AddBoundingBox(block3.model.GetAxisAlignedBox());
    box.AddBoundingBox(block4.model.GetAxisAlignedBox());
    box.AddBoundingBox(block5.model.GetAxisAlignedBox());
    box.AddBoundingBox(block6.model.GetAxisAlignedBox());
    box.AddBoundingBox(block7.model.GetAxisAlignedBox());
    box.AddBoundingBox(block8.model.GetAxisAlignedBox());
    
    if (!camera.GetWorldSpaceFrustum().IntersectBoundingBox(box))
    {
        return;
    }

    if (EnableContainTest && camera.GetWorldSpaceFrustum().ContainingBoundingBox(box))
    {
        RenderBlocksInRangeNoIntersectCheck(minX, maxX, minY, maxY, minZ, maxZ, sorter, camera);
        return;
    }
    // if depth > n, then invoke render in range
    // if any node side reaches 1, invoke render in range
    if (depth >= MaxOctreeDepth || maxX - minX <= MaxOctreeNodeLength || maxY - minY <= MaxOctreeNodeLength || maxZ - minZ <= MaxOctreeNodeLength)
    {
        RenderBlocksInRange(minX, maxX, minY, maxY, minZ, maxZ, sorter, camera);
        return;
    }

    // separate to eight sub space.
    int middleX = (maxX - minX) / 2 + minX;
    int middleY = (maxY - minY) / 2 + minY;
    int middleZ = (maxZ - minZ) / 2 + minZ;
    
    OctreeRenderBlocks(minX, middleX, minY, middleY, minZ, middleZ, depth+1, sorter, camera);
    OctreeRenderBlocks(minX, middleX, minY, middleY, middleZ+1, maxZ, depth+1, sorter, camera);
    OctreeRenderBlocks(middleX+1, maxX, minY, middleY, minZ, middleZ, depth+1, sorter, camera);
    OctreeRenderBlocks(middleX+1, maxX, minY, middleY, middleZ+1, maxZ, depth+1, sorter, camera);
    OctreeRenderBlocks(minX, middleX, middleY+1, maxY, minZ, middleZ, depth+1, sorter, camera);
    OctreeRenderBlocks(minX, middleX, middleY+1, maxY, middleZ+1, maxZ, depth+1, sorter, camera);
    OctreeRenderBlocks(middleX+1, maxX, middleY+1, maxY, minZ, middleZ, depth+1, sorter, camera);
    OctreeRenderBlocks(middleX+1, maxX, middleY+1, maxY, middleZ+1, maxZ, depth+1, sorter, camera);
}


void WorldBlock::Render(Renderer::MeshSorter& sorter, const Camera& camera)
{
    count = 0;
    if (EnableOctree)
    {
        OctreeRenderBlocks(0,worldBlockSize-1,0,worldBlockSize-1, 0, worldBlockDepth-1, 0, sorter,camera);
    }
    else
    {
        count = 0;
        for (int x = 0; x < worldBlockSize; x++)
        {
            for (int y = 0; y < worldBlockSize; y++)
            {
                for (int z = 0; z < worldBlockDepth; z++)
                {
                    Block& block = blocks[x][y][z];
    
                    if (!block.IsNull()
                        && block.adjacent2OuterAir
                        && camera.GetWorldSpaceFrustum().IntersectBoundingBox(block.model.GetAxisAlignedBox()))
                    {
                        count++;
                        block.Render(sorter);
                    }
                }
            }
        }
    }
    std::cout << "render count: " << count << std::endl;
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
