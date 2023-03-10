#include "BlockResourceManager.h"

#include <iostream>
#include <ostream>

#include "ModelLoader.h"
#include "../World/World.h"

namespace BlockResourceManager
{
    bool m_BlocksInitialized = true;
    std::unordered_map<BlockType, std::string> BlockNameMap = {
        {Leaf, "leaf"},
        {Water, "water"},
        {Diamond, "diamond"},
        {Dirt, "dirt"},
        {Grass, "grass"},
        {RedStoneLamp, "redstonelamp"},
        {Stone, "stone"},
        {Torch, "torch"},
        {WoodOak, "wood_oak"},
        {Sand, "sand"},
        {GrassSnow, "grass_snow"},
        {GrassWilt, "grass_wilt"},
        {GrassLeaf, "grass_leaf"}
    };

    std::unordered_set<BlockType> TransparentBlocks = {
        Water,
        GrassLeaf,
    };
    
    std::unordered_map<BlockType, ModelInstance> m_BlockMap;
    std::unordered_map<BlockType, InstancesManager*> BlocksInstancesManagerMap;
    
}

void BlockResourceManager::clearVisibleBlocks()
{
    for (int i =0; i<Air; i++)
    {
        auto type = static_cast<BlockType>(i);
        InstancesManager* manager = BlocksInstancesManagerMap[type];
        manager->visibleBlockNumber = 0;
    }
}

void BlockResourceManager::addBlockIntoManager(BlockType blockType, Math::Vector3 position, float radius)
{
    InstancesManager* manager = BlocksInstancesManagerMap[blockType];
    
    InstanceData data{};
    
    Math::Matrix4 worldMatrix(Math::kIdentity);
    Math::UniformTransform locator(Math::kIdentity);
    locator.SetScale(radius/ getBlockRef(blockType).GetBoundingSphere().GetRadius());
    locator.SetTranslation(position);   
    worldMatrix = Math::Matrix4((Math::AffineTransform)locator) * worldMatrix;

    XMStoreFloat4x4(&data.WorldMatrix, XMMATRIX(worldMatrix));
    Math::Matrix3 worldIT = Math::InverseTranspose(worldMatrix.Get3x3());
    XMStoreFloat3x3(&data.WorldIT, XMMATRIX(worldIT));
    
    manager->mtx.lock();
    if (manager->MAX_BLOCK_NUMBER <= manager->visibleBlockNumber)
    {
        std::cout << "achieve max number" << std::endl;
        manager->mtx.unlock();
        return;
    }
    manager->InstanceBuffer.get()->CopyData(manager->visibleBlockNumber, data);
    manager->visibleBlockNumber++;
    std::atomic_signal_fence(std::memory_order_release);
    manager->mtx.unlock();
}
void BlockResourceManager::initBlocks()
{
    //read resources according to BlockType enum list;
    for (int i = 0; i < BlockType::Air; i++)
    {
        auto type = static_cast<BlockType>(i);
        std::string BlockName = BlockNameMap[type];
        std::string BlockPath = BLOCKS_RESOURCE_PATH + BlockName + "/" + "scene.gltf";

        ModelInstance model{Renderer::LoadModel(Utility::ConvertToWideString(BlockPath), true)};
        std::cout << "ModelCheck blockName: " << BlockName << std::endl; 
        std::cout << "ModelCheck numJoints: "<<model.m_Model->m_NumJoints <<std::endl;
        std::cout << "ModelCheck numMeshes: " << model.m_Model->m_NumMeshes << std::endl; 
        std::cout << "ModelCheck numNodes: "<<model.m_Model->m_NumNodes <<std::endl;
        m_BlockMap[type] = model.m_Model;

        BlocksInstancesManagerMap[type] = new InstancesManager();
        // if (type != Dirt && type !=Grass && type!=Stone && type!=Water)
        // {
        //     BlocksInstancesManagerMap[type].MAX_BLOCK_NUMBER/=10;
        // }
        BlocksInstancesManagerMap[type]->initManager();
        ASSERT_SUCCEEDED(BlocksInstancesManagerMap.size()==BlockType::Air);
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

ModelInstance& BlockResourceManager::getBlockRef(BlockType type)
{
    ASSERT(m_BlocksInitialized);

    if (m_BlockMap.find(type) != m_BlockMap.end())
    {
        return m_BlockMap[type];
    }
    return m_BlockMap[Grass];
}
