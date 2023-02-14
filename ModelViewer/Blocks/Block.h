#pragma once
#include "BlockResourceManager.h"
#include "Math/Vector.h"

class WorldBlock;

class Block
{
    friend WorldBlock;

public:
    Block(Math::Vector3& position, BlockResourceManager::BlockType type, float radius);

    Block()
    {
    }

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
    bool transparent = false;
    bool adjacent2OuterAir = false;
    Math::Vector3 position;
    BlockResourceManager::BlockType blockType;
    Math::BoundingSphere boundingSphere;
    Math::AxisAlignedBox axisAlignedBox;

private:
};
