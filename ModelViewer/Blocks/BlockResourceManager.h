/**
 * 管理所有Block静态资源
 * 1. 负责从静态资源中加载Block
 * 2. 负责所有Block的存储、读取
 */
#pragma once
#include <string>
#include <unordered_map>

#include "Model.h"
#include "UtilUploadBuffer.h"

#define BLOCKS_RESOURCE_PATH "../Resources/Blocks/"

namespace BlockResourceManager
{
    struct InstanceData
    {
        // Matrix4 WorldMatrix;
        // Matrix3 WorldIT;
        DirectX::XMFLOAT4X4 WorldMatrix;
        DirectX::XMFLOAT3X3 WorldIT;
    };

    class InstancesManager;
    extern bool m_BlocksInitialized;

    enum BlockType

    {
        Grass,
        Stone,
        Leaf,
        Dirt,
        Water,
        Wood,
        Diamond,
        RedStoneLamp,
        Torch,
        





        TBall,
        BlocksCount,
    };

    extern std::unordered_map<BlockType, InstancesManager> BlocksInstancesManagerMap;

    extern std::unordered_map<BlockType, ModelInstance> m_BlockMap;

    extern std::unordered_map<BlockType, std::string> BlockNameMap;

    void clearVisibleBlocks();

    void addBlockIntoManager(BlockType blockType, Math::Vector3 position, float radius);

    void initBlocks();

    ModelInstance getBlock(BlockType);
    ModelInstance& getBlockRef(BlockType);

    inline InstancesManager& getManager(BlockType type)
    {
        return BlocksInstancesManagerMap[type];
    }

    class InstancesManager
    {
    public:
        uint32_t MAX_BLOCK_NUMBER = 4096;
        std::vector<InstanceData> InstanceVector{};
        std::unique_ptr<UtilUploadBuffer<InstanceData>> InstanceBuffer = nullptr;
        uint32_t visibleBlockNumber = 0;

        InstancesManager()
        {
        }

        ~InstancesManager()
        {
            InstanceBuffer.release();
        }

        InstancesManager& operator=(const InstancesManager& other)
        {
            return *this;
        }

        void initManager()
        {
            InstanceBuffer = std::make_unique<UtilUploadBuffer<InstanceData>>(Graphics::g_Device, MAX_BLOCK_NUMBER);
            InstanceVector.resize(MAX_BLOCK_NUMBER);
        }
    };
};
