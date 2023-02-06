#include "Block.h"

Block::Block(Math::Vector3& position, BlockResourceManager::BlockType type, float radius): position(position),
    blockType(type), radius(radius)
{
    model = getBlock(type).m_Model;
}

Block& Block::operator=(const Block& block)
{
    this->radius = block.radius;
    this->position = block.position;
    this->blockType = block.blockType;
    this->model = block.model.m_Model;

    return *this;
}

void Block::Update(GraphicsContext& gfxContext, float deltaT)
{
    if (model.IsNull() || !isDirt)
    {
        return;
    }
    model.Update(gfxContext, deltaT);
    isDirt = false;
}

void Block::Render(Renderer::MeshSorter& sorter)
{
    if (model.IsNull())
    {
        return;
    }
    model.Render(sorter);
}

void Block::CleanUp()
{
    model = nullptr;
}
