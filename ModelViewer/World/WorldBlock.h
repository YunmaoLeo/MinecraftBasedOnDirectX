﻿#pragma once
#include <cstdint>
#include <intsafe.h>
#include <iostream>
#include <vector>

#include "ShadowCamera.h"
#include "../Blocks/Block.h"
#include "Math/Vector.h"

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
    }

    WorldBlock(){};

    void InitBlocks();

    void Update(GraphicsContext& gfxContext, float deltaTime);
    void SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>>& blockStatus);
    void SearchBlocksAdjacent2OuterAir();
    void RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, Renderer::MeshSorter& sorter,
                            const Math::Camera& camera);
    void RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                                             Renderer::MeshSorter& sorter, const Math::Camera& camera);
    void OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
                            Renderer::MeshSorter& sorter, const Math::Camera& camera);
    void Render(Renderer::MeshSorter& sorter, const Math::Camera& m_Camera);
    void Render(Renderer::MeshSorter& sorter, const Math::Camera& camera, const ShadowCamera& shadowCamera);
    void CleanUp();


    Math::Vector3 originPoint;
    uint16_t worldBlockSize = 256;
    uint16_t worldBlockDepth = 2;
    std::vector<std::vector<std::vector<Block>>> blocks{};

private:

    int count = 0;
};
