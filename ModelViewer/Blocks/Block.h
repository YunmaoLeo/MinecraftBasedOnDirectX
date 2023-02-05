#pragma once
#include "BlockResourceManager.h"
#include "Math/Vector.h"

class Block
{
public:
    Block(Math::Vector3& position, BlockResourceManager::BlockType type, float radius);
    Block(){}
    Block& operator=(const Block& block);
    
    
    void Update(GraphicsContext& gfxContext, float deltaT);

    void Render(Renderer::MeshSorter& sorter);
    void CleanUp();


    float radius;
    Math::Vector3 position;
    BlockResourceManager::BlockType blockType;
    ModelInstance model;
private:
};
