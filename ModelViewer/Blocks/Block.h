#pragma once
#include "BlockResourceManager.h"
#include "Math/Vector.h"

class Chunk;

class Block
{
    friend Chunk;

public:

    Block()
    {
    }

    Block(Math::Vector3& position, BlockResourceManager::BlockType type, float sideSize);
    Block& operator=(const Block& block);


    void Update(float deltaT);
    void ClearUp();

    bool isTransparent() const
    {
        return transparent;
    }

    bool IsNull();
    
    Math::Vector3 position{};
    float sideSize=0;
    float radius=0;
    BlockResourceManager::BlockType blockType;
    bool hasCheckSibling = false;
    bool isEdgeBlock = false;
    bool transparent = false;
    bool adjacent2Air = false;

private:
};
