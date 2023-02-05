
/**
 * 管理所有Block静态资源
 * 1. 负责从静态资源中加载Block
 * 2. 负责所有Block的存储、读取
 */
#pragma once
#include <string>
#include <unordered_map>

#include "Model.h"

#define BLOCKS_RESOURCE_PATH "../Resources/Blocks/"

namespace BlockResourceManager
{
    extern bool m_BlocksInitialized;

    enum BlockType
    {
        Diamond,
        Stone,
        Wood,
        Dirt,
        RedStoneLamp,
        Torch,


        Grass,
        BlocksCount,
    };

    extern std::unordered_map<BlockType, ModelInstance> m_BlockMap;

    extern std::unordered_map<BlockType, std::string> BlockNameMap;
    
    void initBlocks();

    ModelInstance getBlock(BlockType);
    
};
