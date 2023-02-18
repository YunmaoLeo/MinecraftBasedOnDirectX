﻿#pragma once
#include <cstdint>
#include <intsafe.h>
#include <iostream>
#include <unordered_set>
#include <vector>

#include "OctreeNode.h"
#include "ShadowCamera.h"
#include "../Blocks/Block.h"
#include "Math/Vector.h"

class WorldMap;
struct GlobalConstants;
// WorldBlock 中的每一个单元Block对应一个单位长度，以便于忽略掉长度带来的影响
// Block的坐标以左下方点位作为原点计算，左下方坐标由World创建时传入
class WorldBlock
{
public:
    WorldBlock(Math::Vector3 originPoint, uint16_t blockSize)
        : originPoint(originPoint), worldBlockSize(blockSize)
    {
        blocks.resize(worldBlockSize,
              std::vector<std::vector<Block>>(worldBlockSize,
                                              std::vector<Block>(worldBlockDepth)));
        InitBlocks();
        id = blockId;
        blockId++;
    }

    static int blockId;
    int id;

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

    WorldBlock(){};

    void InitOcclusionQueriesHeaps();
    void CopyDepthBuffer(GraphicsContext& context);
    void ReadDepthBuffer(GraphicsContext& context);
    void RandomlyGenerateBlocks();
    void InitBlocks();

    void Update(float deltaTime);
    int GetBlockOffsetOnHeap(int x, int y, int z) const;
    void SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>>& blockStatus);
    void SearchBlocksAdjacent2OuterAir();
    bool CheckOutOfRange(int x, int y, int z) const;
    void RenderSingleBlock(int x, int y, int z);
    void GetAllBlockOcclusionResult();
    bool GetUnitBlockOcclusionResult(int x, int y, int z) const;
    bool GetUnitBlockOcclusionResultFromVector(int x, int y, int z) const;
    void CheckOcclusion(Renderer::MeshSorter& sorter, GraphicsContext& context, GlobalConstants& globals);
    void RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                             const Math::Camera& camera);
    void RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ);
    bool isAdjacent2OuterAir(int x, int y, int z);
    void CreateOctreeNode(OctreeNode* &node, int minX,int maxX, int minY, int maxY, int minZ, int maxZ, int depth);
    void OctreeRenderBlocks(OctreeNode*& node, const Math::Camera& camera);
    void OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
                            const Math::Camera& camera);
    void CopyOnReadBackBuffer(GraphicsContext& context);
    bool Render(const Math::Camera& camera, GraphicsContext& context);
    void CleanUp();


    Math::Vector3 originPoint;
    OctreeNode* octreeNode;
    uint16_t worldBlockSize = 16;
    uint16_t worldBlockDepth = 64;
    std::vector<std::vector<std::vector<Block>>> blocks{};
    std::vector<std::vector<std::vector<bool>>> blocksOcclusionState{};
    std::vector<BlockPosition> blocksRenderedVector{};
    WorldMap* worldMap;
    int posX;
    int posY;

private:
    ID3D12Resource* m_queryResult;
    ID3D12QueryHeap* m_queryHeap;
    ID3D12Resource* m_queryReadBackBuffer;
    ID3D12Resource* m_depthReadBackBuffer;
    int count = 0;
};
