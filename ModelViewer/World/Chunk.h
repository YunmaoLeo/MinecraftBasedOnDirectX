﻿#pragma once
#include <cstdint>
#include <intsafe.h>
#include <iostream>
#include <unordered_set>
#include <vector>

#include "OctreeNode.h"
#include "ShadowCamera.h"
#include "World.h"
#include "WorldGenerator.h"
#include "../Blocks/Block.h"
#include "Math/Vector.h"

class WorldMap;
struct GlobalConstants;
// WorldBlock 中的每一个单元Block对应一个单位长度，以便于忽略掉长度带来的影响
// Block的坐标以左下方点位作为原点计算，左下方坐标由World创建时传入
class Chunk
{
public:
    Chunk(Math::Vector3 originPoint, uint16_t blockSize)
        : originPoint(originPoint), chunkSize(blockSize)
    {
        blocks.resize(chunkSize,
              std::vector<std::vector<Block>>(chunkSize,
                                              std::vector<Block>(chunkDepth)));
        InitChunks();
        id = blockId;
        blockId++;
    }
    
    static int blockId;
    int id;

    struct BlockPlantInfo
    {
        int height = 0;
        int plantType = 0; //0 for nothing, 1 for grass, 2 for leaf, 3 for wood.
        int woodTopHeight = 0;
    };

    struct BlockPosition
    {
        int x;
        int y;
        int z;

        BlockPosition(int x, int y, int z):x(x),y(y),z(z) {}
        bool operator==(const BlockPosition& pos) const
        {
            return this->x == pos.x && this->y == pos.y && this->z == pos.z;
        }
    };

    Chunk(){};
    static Math::AxisAlignedBox GetScaledSizeAxisBox(Math::Vector3& position)
    {
        float sideSize = World::UnitBlockSize;
        return {position+ Math::Vector3(-sideSize/2,-sideSize/2,-sideSize/2),
        position+Math::Vector3(sideSize/2, sideSize/2,sideSize/2)};
    }
    Math::AxisAlignedBox GetAxisAlignedBox(int x, int y, int z);
    void RandomlyGenerateBlocks();
    static bool Intersect(const Math::Vector3& ori, const Math::Vector3& dir, const Math::AxisAlignedBox& box, float& t);
    void InitChunks();
    bool FindPickBlockInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, Math::Vector3 ori,
                              Math::Vector3 dir, Block*& empty, Block*& entity);
    bool FindPickBlockInOctree(OctreeNode* node, Math::Vector3& ori, Math::Vector3& dir, Block*& empty, Block*& entity);
    std::vector<Block*> getSiblingBlocks(int x, int y, int z);
    bool FindPickBlock(Math::Vector3& ori, Math::Vector3& dir, Block*& empty, Block*& entity);

    void Update(float deltaTime);
    int GetBlockOffsetOnHeap(int x, int y, int z) const;
    void SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>>& blockStatus);
    void SearchBlocksAdjacent2OuterAir();
    bool CheckOutOfRange(int x, int y, int z) const;
    void RenderSingleBlock(int x, int y, int z);
    void RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                             const Math::Camera& camera);
    void RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ);
    bool isAdjacent2OuterAir(int x, int y, int z);
    void CreateOctreeNode(OctreeNode* &node, int minX,int maxX, int minY, int maxY, int minZ, int maxZ, int depth);
    void OctreeRenderBlocks(OctreeNode*& node, const Math::Camera& camera);
    void OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
                            const Math::Camera& camera);
    bool Render(const Math::Camera& camera, GraphicsContext& context);
    void CleanUp();


    Math::Vector3 originPoint;
    OctreeNode* octreeNode;
    uint16_t chunkSize = 16;
    uint16_t chunkDepth = WorldGenerator::WORLD_DEPTH;
    std::vector<std::vector<std::vector<Block>>> blocks{};
    WorldMap* worldMap;
    int posX;
    int posY;

private:
    int count = 0;
};
