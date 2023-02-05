#include "BlockResourceManager.h"

#include <iostream>
#include <ostream>

#include "ModelLoader.h"

namespace BlockResourceManager
{
    bool m_BlocksInitialized = true;
    std::unordered_map<BlockType, std::string> BlockNameMap = {
        {Diamond, "diamond"},
        {Dirt, "dirt"},
        {Grass, "grass"},
        {RedStoneLamp, "redstonelamp"},
        {Stone, "stone"},
        {Torch, "torch"},
        {Wood, "wood"},
    };
    std::unordered_map<BlockType, ModelInstance> m_BlockMap;
    
}

void BlockResourceManager::initBlocks()
{
    //read resources according to BlockType enum list;
    for (int i = 0; i < BlockType::BlocksCount; i++)
    {
        auto type = static_cast<BlockType>(i);
        std::string BlockName = BlockNameMap[type];
        std::string BlockPath = BLOCKS_RESOURCE_PATH + BlockName + "/" + "scene.gltf";

        const ModelInstance model{Renderer::LoadModel(Utility::ConvertToWideString(BlockPath), false)};

        m_BlockMap[type] = model.m_Model;
    }

    m_BlocksInitialized = true;
}

ModelInstance BlockResourceManager::getBlock(BlockType type)
{
    ASSERT(m_BlocksInitialized);

    if (m_BlockMap.find(type) != m_BlockMap.end())
    {
        return m_BlockMap[type];
    }
    return m_BlockMap[Grass];
}
