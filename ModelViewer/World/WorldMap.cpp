﻿#include "WorldMap.h"

#include "World.h"
using namespace Math;

WorldMap::WorldMap(int renderAreaCount, int unitAreaSize, int threadCount)
    : RenderAreaCount(renderAreaCount), UnitAreaSize(unitAreaSize)
{
    thread_pool = new ThreadPool(threadCount);

    //initialize world blocks;
    worldMap = new std::unordered_map<BlockPosition, WorldBlock*, hashName>;
    int renderCount = RenderAreaCount + 2;
    mapOriginPoint = Vector3((-0.5) * UnitAreaSize * World::UnitBlockSize,
                             (-0.5) * UnitAreaSize * World::UnitBlockSize, 0);

    threadResultVector.clear();
    for (int x = 0 - (renderCount / 2); x <= renderCount / 2; x++)
    {
        for (int y = 0 - (renderCount / 2); y <= renderCount / 2; y++)
        {
            // BlockPosition pos{x, y};
            threadResultVector.emplace_back(thread_pool->enqueue([=]
            {
                bool result = createUnitWorldBlock({x,y});
                return result;
            }));
        }
    }
    waitThreadsWorkDone();
    
}

bool WorldMap::createUnitWorldBlock(BlockPosition pos)
{
    auto start = GetTickCount();
    int x = pos.x;
    int y = pos.y;
    Vector3 originPoint = Vector3((x - 0.5) * UnitAreaSize * World::UnitBlockSize,
                                  (y - 0.5) * UnitAreaSize * World::UnitBlockSize, 0);
    WorldBlock* block = new WorldBlock(originPoint, UnitAreaSize);
    block->worldMap = this;
    block->posX = x;
    block->posY = y;
    worldMap->emplace(BlockPosition{x, y}, block);
    auto end = GetTickCount();
    std:: cout << "world block generate time: "<< end-start << "ms"<<std::endl;
    return true;
}

void WorldMap::updateBlockNeedRender(Vector3 position)
{
    BlockPosition pos = getPositionOfCamera(position);
    BlocksNeedRender.clear();
    
    initBufferArea(pos);

    for (int x = pos.x - (RenderAreaCount / 2); x <= pos.x + RenderAreaCount / 2; x++)
    {
        for (int y = pos.y - (RenderAreaCount / 2); y <= pos.y + RenderAreaCount / 2; y++)
        {
            if (worldMap->find(BlockPosition{x, y}) == worldMap->end())
            {
                std::cout << "nothing find in position x: " << x << "y " << y << std::endl;
                continue;
            }
            BlocksNeedRender.emplace_back(worldMap->at(BlockPosition{x, y}));
        }
    }
}

std::vector<WorldBlock*>& WorldMap::getBlocksNeedRender(Vector3 position)
{
    updateBlockNeedRender(position);
    return BlocksNeedRender;
}

void WorldMap::renderVisibleBlocks(Camera& camera, GraphicsContext& context)
{
    BlockResourceManager::clearVisibleBlocks();
    // update blocks
    updateBlockNeedRender(camera.GetPosition());

    //render in multi-threading.
    threadResultVector.clear();

    for (auto& block : BlocksNeedRender)
    {
        threadResultVector.emplace_back(thread_pool->enqueue([&]
        {
            bool result = block->Render(camera, context);
            return result;
        }));
    }

    waitThreadsWorkDone();
}

void WorldMap::waitThreadsWorkDone()
{
    for (auto&& result : threadResultVector)
    {
        result.wait();
    }
}

WorldBlock*& WorldMap::getWorldBlockRef(int x, int y)
{
        return worldMap->at(BlockPosition{x,y});
}

bool WorldMap::hasBlock(int x, int y)
{
    return worldMap->find(BlockPosition{x,y})!=worldMap->end();
}

void WorldMap::initBufferArea(BlockPosition pos)
{
    int renderCount = RenderAreaCount + 2;
    for (int x = pos.x - (renderCount / 2); x <= pos.x + renderCount / 2; x++)
    {
        for (int y = pos.y - (renderCount / 2); y <= pos.y + renderCount / 2; y++)
        {
            BlockPosition blockPos{x,y};
            if (worldMap->find(blockPos)!=worldMap->end()
                || BlocksCreating.find(blockPos)!=BlocksCreating.end())
            {
                continue;
            }
            BlocksCreating.insert(blockPos);
            thread_pool->enqueue([=]
            {
                createUnitWorldBlock(blockPos);
                BlocksCreating.erase(blockPos);
            });
        }
    }
}


WorldMap::BlockPosition WorldMap::getPositionOfCamera(Math::Vector3 position)
{
    int y = float(position.GetZ() - mapOriginPoint.GetY()) / (UnitAreaSize * World::UnitBlockSize);
    int x = float(position.GetX() - mapOriginPoint.GetX()) / (UnitAreaSize * World::UnitBlockSize);
    return {x, y};
}
