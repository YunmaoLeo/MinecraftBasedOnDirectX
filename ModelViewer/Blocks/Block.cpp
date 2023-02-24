#include "Block.h"

Block::Block(Math::Vector3& position, BlockResourceManager::BlockType type, float size, bool empty): position(position),
    blockType(type), sideSize(size)
{
    if (type == BlockResourceManager::GrassLeaf)
    {
        sideSize = sideSize*0.7;
    }
    if (empty)
    {
        isEmpty = true;
    }
    else
    {
        isEmpty = false;
        if (isTransparentBlock(type))
        {
            transparent = true;
        }
    }

    radius = sqrt(std::pow(sideSize, 2)+std::pow(sideSize,2) + std::pow(sideSize,2)) /2;
    // boundingSphere = Math::BoundingSphere(position, radius);
    // Math::AxisAlignedBox box = BlockResourceManager::getBlockRef(type).m_Model->m_BoundingBox;
    axisAlignedBox = Math::AxisAlignedBox(position+ Math::Vector3(-sideSize/2,-sideSize/2,-sideSize/2), position+Math::Vector3(sideSize/2, sideSize/2,sideSize/2));
}

Block& Block::operator=(const Block& block)
{
    this->sideSize = block.sideSize;
    this->radius = block.radius;
    this->position = block.position;
    this->blockType = block.blockType;
    // this->boundingSphere = block.boundingSphere;
    this->axisAlignedBox = block.axisAlignedBox;
    this->isDirt = block.isDirt;
    this->isEmpty = block.isEmpty;
    this->transparent = block.transparent;

    return *this;
}

void Block::Update(float deltaT)
{
    if (isEmpty)
    {
        return;
    }
    isDirt = false;
}

void Block::ClearUp()
{
    isEmpty = true;
}

bool Block::IsNull()
{
    return isEmpty;
}
