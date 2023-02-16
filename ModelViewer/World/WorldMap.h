#pragma once
#include <unordered_map>

#include "ThreadPool.h"
#include "WorldBlock.h"

using namespace Math;
class WorldMap
{
public:
    struct BlockPosition
    {
        int x;
        int y;

        bool operator==(const BlockPosition& pos) const
        {
            return this->x == pos.x && this->y == pos.y;
        }
    };
    WorldMap(int renderAreaCount, int unitAreaSize, int threadCount);
    std::vector<WorldBlock*>& getBlocksNeedRender(Math::Vector3 position);
private:
    struct hashName
    {
        size_t operator()(const BlockPosition& pos) const
        {
            return std::hash<int>()(pos.x) ^ std::hash<int>()(pos.y);
        }
    };
    void initBufferArea(BlockPosition& pos);
    BlockPosition getPositionOfCamera(Math::Vector3 position);
    Vector3 mapOriginPoint;
    // ThreadPool thread_pool;
    int UnitAreaSize;
    int RenderAreaCount;
    std::unordered_map<BlockPosition, WorldBlock*, hashName>* worldMap{};
    std::vector<WorldBlock*> BlocksNeedRender{};


};
