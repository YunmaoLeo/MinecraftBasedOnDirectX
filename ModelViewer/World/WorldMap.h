#pragma once
#include <unordered_map>
#include <unordered_set>

#include "ThreadPool.h"
#include "Chunk.h"

using namespace Math;
class WorldMap
{
public:
    static float minEntityDis;
    static int entityX;
    static int entityY;
    static int entityZ;
    static int entityBlockX ;
    static int entityBlockY ;
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
    bool createUnitWorldBlock(BlockPosition pos);
    void PutBlock(Vector3& ori, Vector3& dir, BlockResourceManager::BlockType type);
    void DeleteBlock(Vector3& ori, Vector3& dir);
    void FindPickBlock(Vector3& ori, Vector3& dir, Block*& empty, Block*& entity);
    std::vector<Chunk*>& getBlocksNeedRender(Math::Vector3 position);
    void renderVisibleBlocks(Camera& camera, GraphicsContext& context);
    void waitThreadsWorkDone();

    Chunk* getWorldBlockRef(int x, int y);
    bool hasBlock(int x, int y);

private:
    struct hashName
    {
        size_t operator()(const BlockPosition& pos) const
        {
            return std::hash<int>()(pos.x) ^ std::hash<int>()(pos.y);
        }
    };
    void initBufferArea(BlockPosition pos);
    void updateBlockNeedRender(Vector3 position);
    BlockPosition getPositionOfCamera(Math::Vector3& position);
    Vector3 mapOriginPoint;
    ThreadPool* thread_pool;
    std::vector<std::future<bool>> threadResultVector;
    std::unordered_set<BlockPosition, hashName> BlocksCreating;
    int UnitAreaSize;
    int RenderAreaCount;
    std::unordered_map<BlockPosition, Chunk*, hashName>* worldMap{};
    std::vector<Chunk*> BlocksNeedRender{};

};
