#include "Chunk.h"

#include <stdbool.h>

#include "BufferManager.h"
#include "Renderer.h"
#include "ShadowCamera.h"
#include "SimplexNoise.h"
#include "World.h"
#include "WorldMap.h"
#include "Math/PerlinNoise.h"
#include "Math/Random.h"
using namespace Math;
using namespace World;

NumVar MaxOctreeDepth("Octree/Depth", 2, 0, 10, 1);
NumVar MaxOctreeNodeLength("Octree/Length", 2, 0, 20, 1);
BoolVar EnableOctree("Octree/EnableOctree", true);
BoolVar EnableContainTest("Octree/EnableContainTest", true);
BoolVar EnableOctreeCompute("Octree/ComputeOptimize", false);

void Chunk::RandomlyGenerateBlocks()
{
    //PerlinNoise noise = PerlinNoise(9);
    std::vector<std::vector<BlockPlantInfo>> plantInfos;
    plantInfos.resize(chunkSize, std::vector<BlockPlantInfo>(chunkSize));
    for (int x = 0; x < chunkSize; x++)
    {
        for (int y = 0; y < chunkSize; y++)
        {
            float xCoor = (float(originPoint.GetX()) + (x + 0.5f) * UnitBlockSize * 1.001) * WorldGenerator::COOR_STEP;
            float yCoor = (float(originPoint.GetY()) + (y + 0.5f) * UnitBlockSize * 1.001) * WorldGenerator::COOR_STEP;

            WorldGenerator::Biomes biomes;
            int realHeight = WorldGenerator::getRealHeightAndBiomes(xCoor, yCoor, biomes);
            
            plantInfos[x][y].height = realHeight;
            for (int z = 0; z < this->chunkDepth - 1; z++)
            {
                Vector3 pointPos = originPoint + Vector3(x + 0.5f, y + 0.5f, z + 0.5f) * UnitBlockSize * 1.001;
                pointPos = Vector3(pointPos.GetX(), pointPos.GetZ(), pointPos.GetY());

                auto blockType = WorldGenerator::getBlockType(xCoor, yCoor, realHeight, biomes, z);
                bool isEmpty = (blockType == BlocksCount);
                blocks[x][y][z] = std::move(Block(pointPos, blockType, UnitBlockSize, isEmpty));
            }

            if (realHeight <= WorldGenerator::SEA_HEIGHT)
            {
                continue;
            }

            // generate biomes condition
            int leafWidth = biomes.TreeHeight/4;
            leafWidth += (biomes.TreeWidth-1);
            if (random.NextFloat() < biomes.PlantDensity)
            {
                if (random.NextFloat() > pow(biomes.PlantDensity, 0.5))
                {
                    plantInfos[x][y].plantType = 1;
                }
                else
                {
                    if (biomes.TreeHeight <=0) continue;
                    if (!(x <= leafWidth-1 || x >= chunkSize - leafWidth || y <= leafWidth-1 || y >= chunkSize - leafWidth))
                    {
                        if (plantInfos[x][y].plantType != 0)
                        {
                            continue;
                        }
                        plantInfos[x][y].plantType = 3;
                        int woodTop = realHeight + biomes.TreeHeight+ (random.NextInt(biomes.TreeHeight/10)) + 2;
                        plantInfos[x][y].woodTopHeight = woodTop;
                        int tw = biomes.TreeWidth - 1;
                        for (int i = x - leafWidth; i <= x + leafWidth; i++)
                        {
                            for (int j = y - leafWidth; j <= y + leafWidth; j++)
                            {
                                if (i <= x+tw && i>=x-tw && j>= y-tw &&j <= y+tw)
                                {
                                    plantInfos[i][j].plantType = 3;
                                    plantInfos[i][j].woodTopHeight = woodTop;
                                    continue;
                                }
                                plantInfos[i][j].plantType = 2;
                                plantInfos[i][j].woodTopHeight = woodTop;
                            }
                        }
                    }
                }
            }
        }
    }

    // adopt plants
    for (int x = 0; x < chunkSize; x++)
    {
        for (int y = 0; y < chunkSize; y++)
        {
            int type = plantInfos[x][y].plantType;
            if (type == 0)
            {
                continue;
            }
            if (type == 1)
            {
                Block& block = blocks[x][y][plantInfos[x][y].height + 1];
                block.blockType = GrassLeaf;
                block.isEmpty = false;
                block.transparent = true;
            }
            if (type == 2)
            {
                for (int h = plantInfos[x][y].woodTopHeight-3 - random.NextInt(1);
                    h<=plantInfos[x][y].woodTopHeight - random.NextInt(1);
                    h++)
                {
                    Block& block = blocks[x][y][h];
                    if (block.blockType==BlocksCount)
                    {
                        if (random.NextFloat() < 0.7)
                        {
                            block.blockType = Leaf;
                            block.isEmpty = false;
                        }
          
                    }
    
                }
            }
            if (type==3)
            {
                BlockPlantInfo& info = plantInfos[x][y];
                for (int h=info.height+1; h<=info.woodTopHeight; h++)
                {
                    Block& block = blocks[x][y][h];
                    block.isEmpty = false;
                    if (h>=info.woodTopHeight-1)
                    {
                        block.blockType = Leaf;
                    }
                    else
                    {
                        block.blockType = WoodOak;
                    }
                }
                auto& blockType = blocks[x][y][info.height].blockType;
                if (blockType ==Grass || blockType == GrassSnow || blockType==GrassWilt)
                {
                    blockType = Dirt;
                }
            }
        }
    }

    for (int z = 0; z < chunkDepth; z++)
    {
        for (int y = 0; y < chunkSize; y++)
        {
            blocks[0][y][z].isEdgeBlock = true;
            blocks[chunkSize - 1][y][z].isEdgeBlock = true;
        }
    }

    for (int z = 0; z < chunkDepth; z++)
    {
        for (int x = 0; x < chunkSize; x++)
        {
            blocks[x][0][z].isEdgeBlock = true;
            blocks[x][chunkSize - 1][z].isEdgeBlock = true;
        }
    }
}

bool Chunk::Intersect(const Vector3& ori, const Vector3& dir, const AxisAlignedBox& box, float& t)
{
    Vector3 ptOnPlane;
    Vector3 min = box.GetMin();
    Vector3 max = box.GetMax();

    if ((float)ori.GetX() < max.GetX()
        && (float)ori.GetX() > min.GetX()
        && (float)ori.GetY() < max.GetY()
        && (float)ori.GetY() > min.GetY()
        && (float)ori.GetZ() < max.GetZ()
        && (float)ori.GetZ() > min.GetZ())
    {
        return true;
    }

    if (dir.GetX() != 0.f)
    {
        if (dir.GetX() > 0.f)
        {
            t = (min.GetX() - ori.GetX()) / dir.GetX();
        }
        else
        {
            t = (max.GetX() - ori.GetX()) / dir.GetX();
        }
        if (t > 0.f)
        {
            ptOnPlane = ori + t * dir;
            if ((float)min.GetY() < ptOnPlane.GetY()
                && (float)ptOnPlane.GetY() < max.GetY()
                && (float)min.GetZ() < ptOnPlane.GetZ()
                && (float)ptOnPlane.GetZ() < max.GetZ())
            {
                return true;
            }
        }
    }

    if (dir.GetY() != 0.f)
    {
        if (dir.GetY() > 0.f)
        {
            t = (min.GetY() - ori.GetY()) / dir.GetY();
        }
        else
        {
            t = (max.GetY() - ori.GetY()) / dir.GetY();
        }
        if (t > 0.f)
        {
            ptOnPlane = ori + t * dir;
            if ((float)min.GetZ() < ptOnPlane.GetZ()
                && (float)ptOnPlane.GetZ() < max.GetZ()
                && (float)min.GetX() < ptOnPlane.GetX()
                && (float)ptOnPlane.GetX() < max.GetX())
            {
                return true;
            }
        }
    }

    if (dir.GetZ() != 0.f)
    {
        if (dir.GetZ() > 0.f)
        {
            t = (min.GetZ() - ori.GetZ()) / dir.GetZ();
        }
        else
        {
            t = (max.GetZ() - ori.GetZ()) / dir.GetZ();
        }
        if (t > 0.f)
        {
            ptOnPlane = ori + t * dir;
            if ((float)min.GetX() < ptOnPlane.GetX()
                && (float)ptOnPlane.GetX() < max.GetX()
                && (float)min.GetY() < ptOnPlane.GetY()
                && (float)ptOnPlane.GetY() < max.GetY())
            {
                return true;
            }
        }
    }
    return false;
}

void Chunk::InitChunks()
{
    RandomlyGenerateBlocks();
    SearchBlocksAdjacent2OuterAir();
    // InitOcclusionQueriesHeaps();
    CreateOctreeNode(this->octreeNode, 0, this->chunkSize - 1, 0, this->chunkSize - 1, 0,
                     this->chunkDepth - 1, 0);
}


bool Chunk::FindPickBlockInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, Vector3 ori,
                                 Vector3 dir, Block*& empty, Block*& entity)
{
    float t;
    bool result = false;
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                if (blocks[x][y][z].IsNull() || !blocks[x][y][z].adjacent2Air)
                {
                    continue;
                }
                if (Intersect(ori, dir, blocks[x][y][z].axisAlignedBox, t))
                {
                    if (t < WorldMap::minEntityDis)
                    {
                        WorldMap::minEntityDis = t;
                        entity = &blocks[x][y][z];
                        WorldMap::entityX = x;
                        WorldMap::entityY = y;
                        WorldMap::entityZ = z;
                        WorldMap::entityBlockX = posX;
                        WorldMap::entityBlockY = posY;
                    }
                    result = true;
                }
            }
        }
    }
    return result;
}

bool Chunk::FindPickBlockInOctree(OctreeNode* node, Vector3& ori, Vector3& dir, Block*& empty, Block*& entity)
{
    float t;
    if (!Intersect(ori, dir, node->box, t))
    {
        return false;
    }
    if (node->isLeafNode)
    {
        return FindPickBlockInRange(node->minX, node->maxX,
                                    node->minY, node->maxY,
                                    node->minZ, node->maxZ,
                                    ori, dir,
                                    empty, entity);
    }
    bool result = false;

    result |= FindPickBlockInOctree(node->leftBottomBack, ori, dir, empty, entity);
    result |= FindPickBlockInOctree(node->leftTopBack, ori, dir, empty, entity);
    result |= FindPickBlockInOctree(node->rightBottomBack, ori, dir, empty, entity);
    result |= FindPickBlockInOctree(node->rightTopBack, ori, dir, empty, entity);
    result |= FindPickBlockInOctree(node->leftBottomFront, ori, dir, empty, entity);
    result |= FindPickBlockInOctree(node->leftTopFront, ori, dir, empty, entity);
    result |= FindPickBlockInOctree(node->rightBottomFront, ori, dir, empty, entity);
    result |= FindPickBlockInOctree(node->rightTopFront, ori, dir, empty, entity);

    return result;
}

std::vector<Block*> Chunk::getSiblingBlocks(int x, int y, int z)
{
    std::vector<Block*> result;
    Block& block = blocks[x][y][z];
    if (x != 0)
    {
        result.push_back(&blocks[x - 1][y][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (x != chunkSize - 1)
    {
        result.push_back(&blocks[x + 1][y][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (y != chunkSize - 1)
    {
        result.push_back(&blocks[x][y + 1][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (y != 0)
    {
        result.push_back(&blocks[x][y - 1][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (z != 0)
    {
        result.push_back(&blocks[x][y][z - 1]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (z != chunkDepth - 1)
    {
        result.push_back(&blocks[x][y][z + 1]);
    }
    else
    {
        result.push_back(nullptr);
    }


    if (block.isEdgeBlock)
    {
        if (x == 0)
        {
            if (worldMap->hasBlock(posX - 1, posY))
            {
                result[0] = (&worldMap->getWorldBlockRef(posX - 1, posY)->blocks[chunkSize - 1][y][z]);
            }
            else
            {
                result[0] = nullptr;
            }
        }
        if (x == chunkSize - 1)
        {
            if (worldMap->hasBlock(posX + 1, posY))
            {
                result[1] = (&worldMap->getWorldBlockRef(posX + 1, posY)->blocks[0][y][z]);
            }
            else
            {
                result[1] = nullptr;
            }
        }
        if (y == 0)
        {
            if (worldMap->hasBlock(posX, posY - 1))
            {
                result[3] = (&worldMap->getWorldBlockRef(posX, posY - 1)->blocks[x][chunkSize - 1][z]);
            }
            else
            {
                result[3] = nullptr;
            }
        }
        if (y == chunkSize - 1)
        {
            if (worldMap->hasBlock(posX, posY + 1))
            {
                result[2] = (&worldMap->getWorldBlockRef(posX, posY + 1)->blocks[x][0][z]);
            }
            else
            {
                result[2] = nullptr;
            }
        }
    }
    return result;
}

bool Chunk::FindPickBlock(Vector3& ori, Vector3& dir, Block*& empty, Block*& entity)
{
    return FindPickBlockInOctree(octreeNode, ori, dir, empty, entity);
}

void Chunk::Update(float deltaTime)
{
    for (int x = 0; x < chunkSize; x++)
    {
        for (int y = 0; y < chunkSize; y++)
        {
            for (int z = 0; z < chunkDepth; z++)
            {
                blocks[x][y][z].Update(deltaTime);
            }
        }
    }
}

int Chunk::GetBlockOffsetOnHeap(int x, int y, int z) const
{
    return z * (chunkSize * chunkSize) + y * chunkSize + x;
}

void Chunk::SearchBlocksAdjacent2OuterAir()
{
    std::vector<std::vector<std::vector<int>>> blocksStatus(chunkSize,
                                                            std::vector<std::vector<int>>(chunkSize,
                                                                std::vector<int>(chunkDepth)));

    // Assign outermost blocks as adjacent to outer air.
    // Outermost blocks has a same attribute, one of x,y equals 0 or worldBlockSize-1
    //                                  or z equals 0 or worldBlockDepth-1;
    // for (int y = 0; y < worldBlockSize; y++)
    // {
    //     for (int z = 0; z < worldBlockDepth; z++)
    //     {
    //         blocks[0][y][z].adjacent2OuterAir = true;
    //         blocks[worldBlockSize - 1][y][z].adjacent2OuterAir = true;
    //     }
    // }
    //
    // for (int x = 0; x < worldBlockSize; x++)
    // {
    //     for (int z = 0; z < worldBlockDepth; z++)
    //     {
    //         blocks[x][0][z].adjacent2OuterAir = true;
    //         blocks[x][worldBlockSize - 1][z].adjacent2OuterAir = true;
    //     }
    // }

    // for (int x = 0; x < worldBlockSize; x++)
    // {
    //     for (int y = 0; y < worldBlockSize; y++)
    //     {
    //         // blocks[x][y][0].adjacent2OuterAir = true;
    //         blocks[x][y][worldBlockDepth - 1].adjacent2Air = true;
    //     }
    // }
    for (int x = 0; x < chunkSize; x++)
    {
        for (int y = 0; y < chunkSize; y++)
        {
            for (int z = chunkDepth - 1; z >= 0; z--)
            {
                if (blocks[x][y][z].IsNull() || blocks[x][y][z].isTransparent())
                {
                    SpreadAdjacent2OuterAir(x + 1, y, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x - 1, y, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y + 1, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y - 1, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y, z + 1, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y, z - 1, blocksStatus);
                }
            }
        }
    }
}

bool Chunk::CheckOutOfRange(int x, int y, int z) const
{
    return x < 0 || x >= chunkSize || y < 0 || y >= chunkSize || z < 0 || z >= chunkDepth;
}

void Chunk::RenderSingleBlock(int x, int y, int z)
{
    count++;
    Block& block = blocks[x][y][z];
    BlockResourceManager::addBlockIntoManager(block.blockType, block.position, block.radius);
}

void Chunk::RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                                const Camera& camera)
{
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                Block& block = blocks[x][y][z];

                if (!block.IsNull()
                    && isAdjacent2OuterAir(x, y, z)
                    && camera.GetWorldSpaceFrustum().IntersectBoundingBox(block.axisAlignedBox)
                )
                {
                    RenderSingleBlock(x, y, z);
                }
            }
        }
    }
}

void Chunk::RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ)
{
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                Block& block = blocks[x][y][z];

                if (!block.IsNull()
                    && isAdjacent2OuterAir(x, y, z))
                {
                    RenderSingleBlock(x, y, z);
                }
            }
        }
    }
}

bool Chunk::isAdjacent2OuterAir(int x, int y, int z)
{
    Block& block = blocks[x][y][z];
    if (block.adjacent2Air)
    {
        return true;
    }
    bool hasSiblingVisibleAir = false;
    if (block.isEdgeBlock &&
        !block.hasCheckSibling)
    {
        if (x == 0)
        {
            if (worldMap->hasBlock(posX - 1, posY))
            {
                Chunk* block = worldMap->getWorldBlockRef(posX - 1, posY);
                Block& siblingBlock = block->blocks[chunkSize - 1][y][z];
                if (siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }

        if (y == 0)
        {
            if (worldMap->hasBlock(posX, posY - 1))
            {
                Chunk* block = worldMap->getWorldBlockRef(posX, posY - 1);
                Block& siblingBlock = block->blocks[x][chunkSize - 1][z];
                if (siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }

        if (x == chunkSize - 1)
        {
            if (worldMap->hasBlock(posX + 1, posY))
            {
                Chunk* block = worldMap->getWorldBlockRef(posX + 1, posY);
                Block& siblingBlock = block->blocks[0][y][z];
                if (siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }

        if (y == chunkSize - 1)
        {
            if (worldMap->hasBlock(posX, posY + 1))
            {
                Chunk* block = worldMap->getWorldBlockRef(posX, posY + 1);
                Block& siblingBlock = block->blocks[x][0][z];
                if (siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }
        block.hasCheckSibling = true;
    }

    if (hasSiblingVisibleAir)
    {
        block.adjacent2Air = true;
        return true;
    }
    return false;
}

void Chunk::CreateOctreeNode(OctreeNode* & node, int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                             int depth)
{
    if (minX >= maxX || minY >= maxY || minZ >= maxZ)
    {
        return;
    }
    int maxDepth = 3;
    bool isLeafNode = depth == maxDepth ? true : false;
    node = new OctreeNode(minX, maxX, minY, maxY, minZ, maxZ, isLeafNode);

    AxisAlignedBox box;

    for (int x = minX; x <= maxX; x += (maxX - minX))
    {
        for (int y = minY; y <= maxY; y += (maxY - minY))
        {
            for (int z = minZ; z <= maxZ; z += (maxZ - minZ))
            {
                Vector3 worldPoint = {originPoint.GetX(), originPoint.GetZ(), originPoint.GetY()};
                Vector3 minPoint = worldPoint + (Vector3(x, z, y) * UnitBlockSize);
                Vector3 maxPoint = worldPoint + (Vector3(x + 1, z + 1, y + 1) * UnitBlockSize);
                box = std::move(box.Union({minPoint, maxPoint}));
            }
        }
    }
    node->box = std::move(box);

    if (depth == maxDepth)
    {
        return;
    }

    // separate to eight sub space.
    int middleX = (maxX - minX) / 2 + minX;
    int middleY = (maxY - minY) / 2 + minY;
    int middleZ = (maxZ - minZ) / 2 + minZ;

    CreateOctreeNode(node->leftBottomBack, minX, middleX, minY, middleY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->leftTopBack, minX, middleX, minY, middleY, middleZ + 1, maxZ, depth + 1);
    CreateOctreeNode(node->rightBottomBack, middleX + 1, maxX, minY, middleY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->rightTopBack, middleX + 1, maxX, minY, middleY, middleZ + 1, maxZ, depth + 1);
    CreateOctreeNode(node->leftBottomFront, minX, middleX, middleY + 1, maxY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->leftTopFront, minX, middleX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1);
    CreateOctreeNode(node->rightBottomFront, middleX + 1, maxX, middleY + 1, maxY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->rightTopFront, middleX + 1, maxX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1);
}

void Chunk::OctreeRenderBlocks(OctreeNode* & node, const Camera& camera)
{
    AxisAlignedBox box = node->box;

    if (!camera.GetWorldSpaceFrustum().IntersectBoundingBox(box))
    {
        return;
    }

    int minX = node->minX;
    int maxX = node->maxX;
    int minY = node->minY;
    int maxY = node->maxY;
    int minZ = node->minZ;
    int maxZ = node->maxZ;

    if (EnableContainTest && camera.GetWorldSpaceFrustum().ContainingBoundingBox(box))
    {
        RenderBlocksInRangeNoIntersectCheck(minX, maxX, minY, maxY, minZ, maxZ);
        return;
    }
    // if depth > n, then invoke render in range
    // if any node side reaches 1, invoke render in range
    if (node->isLeafNode || maxX - minX <= MaxOctreeNodeLength || maxY - minY <= MaxOctreeNodeLength || maxZ -
        minZ <= MaxOctreeNodeLength)
    {
        RenderBlocksInRange(minX, maxX, minY, maxY, minZ, maxZ, camera);
        return;
    }

    // separate to eight sub space.
    OctreeRenderBlocks(node->leftBottomBack, camera);
    OctreeRenderBlocks(node->leftBottomFront, camera);
    OctreeRenderBlocks(node->leftTopBack, camera);
    OctreeRenderBlocks(node->leftTopFront, camera);
    OctreeRenderBlocks(node->rightBottomBack, camera);
    OctreeRenderBlocks(node->rightBottomFront, camera);
    OctreeRenderBlocks(node->rightTopBack, camera);
    OctreeRenderBlocks(node->rightTopFront, camera);
}

void Chunk::OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
                               const Camera& camera)
{
    //check bounding box intersection
    AxisAlignedBox box;

    Block& block1 = blocks[minX][minY][minZ];
    Block& block2 = blocks[minX][maxY][minZ];
    Block& block3 = blocks[minX][minY][maxZ];
    Block& block4 = blocks[minX][maxY][maxZ];
    Block& block5 = blocks[maxX][minY][minZ];
    Block& block6 = blocks[maxX][maxY][minZ];
    Block& block7 = blocks[maxX][minY][maxZ];
    Block& block8 = blocks[maxX][maxY][maxZ];


    box.AddBoundingBox(block1.axisAlignedBox);
    box.AddBoundingBox(block2.axisAlignedBox);
    box.AddBoundingBox(block3.axisAlignedBox);
    box.AddBoundingBox(block4.axisAlignedBox);
    box.AddBoundingBox(block5.axisAlignedBox);
    box.AddBoundingBox(block6.axisAlignedBox);
    box.AddBoundingBox(block7.axisAlignedBox);
    box.AddBoundingBox(block8.axisAlignedBox);

    if (!camera.GetWorldSpaceFrustum().IntersectBoundingBox(box))
    {
        return;
    }

    if (EnableContainTest && camera.GetWorldSpaceFrustum().ContainingBoundingBox(box))
    {
        RenderBlocksInRangeNoIntersectCheck(minX, maxX, minY, maxY, minZ, maxZ);
        return;
    }
    // if depth > n, then invoke render in range
    // if any node side reaches 1, invoke render in range
    if (depth >= MaxOctreeDepth || maxX - minX <= MaxOctreeNodeLength || maxY - minY <= MaxOctreeNodeLength || maxZ -
        minZ <= MaxOctreeNodeLength)
    {
        RenderBlocksInRange(minX, maxX, minY, maxY, minZ, maxZ, camera);
        return;
    }

    // separate to eight sub space.
    int middleX = (maxX - minX) / 2 + minX;
    int middleY = (maxY - minY) / 2 + minY;
    int middleZ = (maxZ - minZ) / 2 + minZ;

    OctreeRenderBlocks(minX, middleX, minY, middleY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(minX, middleX, minY, middleY, middleZ + 1, maxZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, minY, middleY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, minY, middleY, middleZ + 1, maxZ, depth + 1, camera);
    OctreeRenderBlocks(minX, middleX, middleY + 1, maxY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(minX, middleX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, middleY + 1, maxY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1, camera);
}

bool Chunk::Render(const Camera& camera, GraphicsContext& context)
{
    // blocksRenderedVector.clear();
    count = 0;
    if (EnableOctree)
    {
        if (EnableOctreeCompute)
        {
            OctreeRenderBlocks(this->octreeNode, camera);
        }
        else
        {
            OctreeRenderBlocks(0, chunkSize - 1, 0, chunkSize - 1, 0, chunkDepth - 1, 0, camera);
        }
    }
    else
    {
        count = 0;
        for (int x = 0; x < chunkSize; x++)
        {
            for (int y = 0; y < chunkSize; y++)
            {
                for (int z = 0; z < chunkDepth; z++)
                {
                    Block& block = blocks[x][y][z];
                    if (!block.IsNull()
                        && block.adjacent2Air
                        //&& camera.GetWorldSpaceFrustum().IntersectBoundingBox(block.axisAlignedBox)
                    )
                    {
                        RenderSingleBlock(x, y, z);
                    }
                }
            }
        }
    }
    return false;
}

void Chunk::CleanUp()
{
    for (auto blocksX : blocks)
    {
        for (auto blocksY : blocksX)
        {
            for (auto block : blocksY)
            {
                block.ClearUp();
            }
        }
    }
}

void Chunk::SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>>& blockStatus)
{
    // out of boundary
    if (x < 0 || x > chunkSize - 1 || y < 0 || y > chunkSize - 1 || z < 0 || z > chunkDepth - 1)
    {
        return;
    }

    if (blockStatus[x][y][z])
    {
        return;
    }

    blocks[x][y][z].adjacent2Air = true;


    blockStatus[x][y][z] = 1;
    //
    // if (blocks[x][y][z].IsNull() || blocks[x][y][z].isTransparent())
    // {
    //     SpreadAdjacent2OuterAir(x + 1, y, z, blockStatus);
    //     SpreadAdjacent2OuterAir(x - 1, y, z, blockStatus);
    //     SpreadAdjacent2OuterAir(x, y + 1, z, blockStatus);
    //     SpreadAdjacent2OuterAir(x, y - 1, z, blockStatus);
    //     // SpreadAdjacent2OuterAir(x, y, z + 1, blockStatus);
    //     SpreadAdjacent2OuterAir(x, y, z - 1, blockStatus);
    // }
}
