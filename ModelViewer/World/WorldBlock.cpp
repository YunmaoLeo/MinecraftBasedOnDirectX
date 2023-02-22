﻿#include "WorldBlock.h"

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


void WorldBlock::RandomlyGenerateBlocks()
{
    //PerlinNoise noise = PerlinNoise(9);
    SimplexNoise lowNoise(0.8, 1, 0.4, 0.5);
    SimplexNoise highNoise(0.2, 1, 0.5, 0.5);
    SimplexNoise hillNoise(0.3, 1, 0.5, 0.5);
    SimplexNoise blendNoise(0.48, 1, 0.5, 0.3);
    RandomNumberGenerator generator;
    generator.SetSeed(1);
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            //float(originPoint.GetX())+(x+0.5f)*UnitBlockSize*1.001,float(originPoint.GetY())+(y+0.5f)*UnitBlockSize*1.001, 0.8
            //double height = noise.noise((float(originPoint.GetX())+(x+0.5f)*UnitBlockSize*1.001)*0.001,(float(originPoint.GetY())+(y+0.5f)*UnitBlockSize*1.001)*0.001, 0.8);
            double step = 0.0006;
            double low = lowNoise.fractal(4, (float(originPoint.GetX()) + (x + 0.5f) * UnitBlockSize * 1.001) * step,
                                          (float(originPoint.GetY()) + (y + 0.5f) * UnitBlockSize * 1.001) * step);
            double high = highNoise.fractal(3, (float(originPoint.GetX()) + (x + 0.5f) * UnitBlockSize * 1.001) * step,
                                            (float(originPoint.GetY()) + (y + 0.5f) * UnitBlockSize * 1.001) * step);
            double blend = blendNoise.fractal(
                8, (float(originPoint.GetX()) + (x + 0.5f) * UnitBlockSize * 1.001) * step,
                (float(originPoint.GetY()) + (y + 0.5f) * UnitBlockSize * 1.001) * step);
            // double hill = hillNoise.fractal(5, (float(originPoint.GetX())+(x+0.5f)*UnitBlockSize*1.001)*0.0005,(float(originPoint.GetY())+(y+0.5f)*UnitBlockSize*1.001)*0.0005);


            low /= 16;
            //hill /= 2;
            blend += 1;

            // if (blend > 1.5)
            // {
            //     blend *= ((2-blend)/2 + 1);
            // }

            double value = ((blend) * high + (2 - blend) * low) / 2;
            // if (blend < 0.3)
            // {
            //     value = ((blend+0.07) * high - (2-blend-0.07) * low)/2;
            // }

            int realHeight = this->worldBlockDepth * (value + 1) / 2;
            if (realHeight >= this->worldBlockDepth) realHeight = this->worldBlockDepth - 1;
            if (realHeight < 0) realHeight = 0;
            for (int z = 0; z < this->worldBlockDepth - 1; z++)
            {
                if (z >= realHeight)
                {
                    Vector3 pointPos = originPoint + Vector3(x + 0.5f, y + 0.5f, z + 0.5f) * UnitBlockSize * 1.001;
                    pointPos = Vector3(pointPos.GetX(), pointPos.GetZ(), pointPos.GetY());
                    blocks[x][y][z] = Block(pointPos, BlockResourceManager::BlockType(0), UnitBlockSize, true);
                    continue;
                }
                BlockType type;
                // if (z == realHeight-1)
                // {
                //     type = Dirt;
                // }
                if (float(z) / float(realHeight) < 0.5 + generator.NextInt(-1, 1) * 0.15)
                {
                    type = Stone;
                }
                else
                {
                    if (realHeight < 22)
                    {
                        type = Water;
                    }
                    else if (realHeight < 36)
                    {
                        type = Grass;
                    }
                    else
                    {
                        type = Stone;
                    }
                }
                Vector3 pointPos = originPoint + Vector3(x + 0.5f, y + 0.5f, z + 0.5f) * UnitBlockSize * 1.001;
                pointPos = Vector3(pointPos.GetX(), pointPos.GetZ(), pointPos.GetY());
                blocks[x][y][z] = Block(pointPos, BlockResourceManager::BlockType(type), UnitBlockSize, false);
            }
        }
    }
    for (int z = 0; z < worldBlockDepth; z++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            blocks[0][y][z].isEdgeBlock = true;
            blocks[worldBlockSize - 1][y][z].isEdgeBlock = true;
        }
    }

    for (int z = 0; z < worldBlockDepth; z++)
    {
        for (int x = 0; x < worldBlockSize; x++)
        {
            blocks[x][0][z].isEdgeBlock = true;
            blocks[x][worldBlockSize - 1][z].isEdgeBlock = true;
        }
    }
}

bool WorldBlock::Intersect(const Vector3& ori, const Vector3& dir, const AxisAlignedBox& box, float& t)
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

void WorldBlock::InitBlocks()
{
    RandomlyGenerateBlocks();
    SearchBlocksAdjacent2OuterAir();
    // InitOcclusionQueriesHeaps();
    CreateOctreeNode(this->octreeNode, 0, this->worldBlockSize - 1, 0, this->worldBlockSize - 1, 0,
                     this->worldBlockDepth - 1, 0);
}


bool WorldBlock::FindPickBlockInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, Vector3 ori,
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
                if (blocks[x][y][z].IsNull() || !blocks[x][y][z].adjacent2OuterAir)
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

bool WorldBlock::FindPickBlockInOctree(OctreeNode* node, Vector3& ori, Vector3& dir, Block*& empty, Block*& entity)
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

std::vector<Block*> WorldBlock::getSiblingBlocks(int x, int y, int z)
{
    std::vector<Block*> result;
    Block& block = blocks[x][y][z];
    if (x!=0)
    {
        result.push_back(&blocks[x - 1][y][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (x!=worldBlockSize-1)
    {
        result.push_back(&blocks[x + 1][y][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (y!=worldBlockSize-1)
    {
        result.push_back(&blocks[x][y + 1][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (y!=0)
    {
        result.push_back(&blocks[x][y - 1][z]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (z!=0)
    {
        result.push_back(&blocks[x][y][z - 1]);
    }
    else
    {
        result.push_back(nullptr);
    }
    if (z!=worldBlockDepth-1)
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
                result[0] = (&worldMap->getWorldBlockRef(posX - 1, posY)->blocks[worldBlockSize - 1][y][z]);
            }
            else
            {
                result[0] = nullptr;
            }
        }
        if (x == worldBlockSize - 1)
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
                result[3] = (&worldMap->getWorldBlockRef(posX, posY - 1)->blocks[x][worldBlockSize - 1][z]);
            }
            else
            {
                result[3] = nullptr;
            }
        }
        if (y == worldBlockSize - 1)
        {
            if (worldMap->hasBlock(posX, posY + 1))
            {
                result[2] = (&worldMap->getWorldBlockRef(posX, posY + 1)->blocks[x][0][z]);
            }
            else
            {
                result[2]=nullptr;
            }
        }
    }
    return result;
}

bool WorldBlock::FindPickBlock(Vector3& ori, Vector3& dir, Block*& empty, Block*& entity)
{
    return FindPickBlockInOctree(octreeNode, ori, dir, empty, entity);
}

void WorldBlock::Update(float deltaTime)
{
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                blocks[x][y][z].Update(deltaTime);
            }
        }
    }
}

int WorldBlock::GetBlockOffsetOnHeap(int x, int y, int z) const
{
    return z * (worldBlockSize * worldBlockSize) + y * worldBlockSize + x;
}

void WorldBlock::SearchBlocksAdjacent2OuterAir()
{
    std::vector<std::vector<std::vector<int>>> blocksStatus(worldBlockSize,
                                                            std::vector<std::vector<int>>(worldBlockSize,
                                                                std::vector<int>(worldBlockDepth)));

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

    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            // blocks[x][y][0].adjacent2OuterAir = true;
            blocks[x][y][worldBlockDepth - 1].adjacent2OuterAir = true;
        }
    }
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = worldBlockDepth - 1; z >= 0; z--)
            {
                if (blocks[x][y][z].IsNull() && blocks[x][y][z].adjacent2OuterAir)
                {
                    SpreadAdjacent2OuterAir(x + 1, y, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x - 1, y, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y + 1, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y - 1, z, blocksStatus);
                    // SpreadAdjacent2OuterAir(x, y, z + 1, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y, z - 1, blocksStatus);
                }
            }
        }
    }
}

bool WorldBlock::CheckOutOfRange(int x, int y, int z) const
{
    return x < 0 || x >= worldBlockSize || y < 0 || y >= worldBlockSize || z < 0 || z >= worldBlockDepth;
}

void WorldBlock::RenderSingleBlock(int x, int y, int z)
{
    count++;
    Block& block = blocks[x][y][z];
    BlockResourceManager::addBlockIntoManager(block.blockType, block.position, block.radius);
}

void WorldBlock::RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
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

void WorldBlock::RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ)
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

bool WorldBlock::isAdjacent2OuterAir(int x, int y, int z)
{
    Block& block = blocks[x][y][z];
    if (block.adjacent2OuterAir)
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
                WorldBlock* block = worldMap->getWorldBlockRef(posX - 1, posY);
                Block& siblingBlock = block->blocks[worldBlockSize - 1][y][z];
                if (siblingBlock.adjacent2OuterAir && siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }

        if (y == 0)
        {
            if (worldMap->hasBlock(posX, posY - 1))
            {
                WorldBlock* block = worldMap->getWorldBlockRef(posX, posY - 1);
                Block& siblingBlock = block->blocks[x][worldBlockSize - 1][z];
                if (siblingBlock.adjacent2OuterAir && siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }

        if (x == worldBlockSize - 1)
        {
            if (worldMap->hasBlock(posX + 1, posY))
            {
                WorldBlock* block = worldMap->getWorldBlockRef(posX + 1, posY);
                Block& siblingBlock = block->blocks[0][y][z];
                if (siblingBlock.adjacent2OuterAir && siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }

        if (y == worldBlockSize - 1)
        {
            if (worldMap->hasBlock(posX, posY + 1))
            {
                WorldBlock* block = worldMap->getWorldBlockRef(posX, posY + 1);
                Block& siblingBlock = block->blocks[x][0][z];
                if (siblingBlock.adjacent2OuterAir && siblingBlock.IsNull())
                {
                    hasSiblingVisibleAir = true;
                }
            }
        }
        block.hasCheckSibling = true;
    }

    if (hasSiblingVisibleAir)
    {
        block.adjacent2OuterAir = true;
        return true;
    }
    return false;
}

void WorldBlock::CreateOctreeNode(OctreeNode* & node, int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
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

void WorldBlock::OctreeRenderBlocks(OctreeNode* & node, const Camera& camera)
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

void WorldBlock::OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
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

bool WorldBlock::Render(const Camera& camera, GraphicsContext& context)
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
            OctreeRenderBlocks(0, worldBlockSize - 1, 0, worldBlockSize - 1, 0, worldBlockDepth - 1, 0, camera);
        }
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

void WorldBlock::CleanUp()
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

void WorldBlock::SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>>& blockStatus)
{
    // out of boundary
    if (x < 0 || x > worldBlockSize - 1 || y < 0 || y > worldBlockSize - 1 || z < 0 || z > worldBlockDepth - 1)
    {
        return;
    }

    if (blockStatus[x][y][z])
    {
        return;
    }

    blocks[x][y][z].adjacent2OuterAir = true;


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
