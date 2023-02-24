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

    Block(Math::Vector3& position, BlockResourceManager::BlockType type, float sideSize, bool empty);
    Block& operator=(const Block& block);


    void Update(float deltaT);
    void ClearUp();

    bool isTransparent() const
    {
        return transparent;
    }

    bool IsNull();

    float sideSize;
    float radius;
    bool isEmpty = true;
    bool isDirt = true;
    bool hasCheckSibling = false;
    bool isEdgeBlock = false;
    bool transparent = false;
    bool adjacent2Air = false;
    Math::Vector3 position;
    BlockResourceManager::BlockType blockType;
    // Math::BoundingSphere boundingSphere;
    Math::AxisAlignedBox axisAlignedBox;

    Math::Matrix4* worldMatrix;
    Math::Matrix4* worldIT;

private:
};
