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


    void Update(GraphicsContext& gfxContext, float deltaT);

    void Render(Renderer::MeshSorter& sorter);
    void CleanUp();

    bool IsNull() const
    {
        return model.IsNull();
    }

    bool isTransparent() const
    {
        return transparent;
    }

private:
    float radius;
    bool isDirt = true;
    bool transparent = false;
    bool adjacent2OuterAir = false;
    Math::Vector3 position;
    BlockResourceManager::BlockType blockType;
    ModelInstance model;
};
