#include "WorldBlock.h"

#include "Renderer.h"
#include "ShadowCamera.h"
#include "World.h"
#include "Math/Random.h"
using namespace Math;
using namespace World;

NumVar MaxOctreeDepth("Octree/Depth", 2, 0, 10, 1);
NumVar MaxOctreeNodeLength("Octree/Length", 2, 0, 20, 1);
BoolVar EnableOctree("Octree/EnableOctree", true);
BoolVar EnableContainTest("Octree/EnableContainTest", true);
BoolVar EnableOcclusion("Model/EnableOcclusion", true);

void WorldBlock::InitOcclusionQueriesHeaps()
{
    D3D12_QUERY_HEAP_DESC occlusionQueryHeapDesc = {};
    const uint16_t blockCount = worldBlockSize * worldBlockSize * worldBlockDepth;
    occlusionQueryHeapDesc.Count = blockCount;
    occlusionQueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;

    Graphics::g_Device->CreateQueryHeap(&occlusionQueryHeapDesc, IID_PPV_ARGS(&m_queryHeap));

    CD3DX12_HEAP_PROPERTIES resultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    auto queryBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * blockCount);

    Graphics::g_Device->CreateCommittedResource(
        &resultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &queryBufferDesc,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        nullptr,
        IID_PPV_ARGS(&m_queryResult)
    );

    CD3DX12_HEAP_PROPERTIES readBackHeapProps(D3D12_HEAP_TYPE_READBACK);
    Graphics::g_Device->CreateCommittedResource(
        &readBackHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &queryBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_queryReadBackBuffer));
}

void WorldBlock::RandomlyGenerateBlocks()
{
    RandomNumberGenerator generator;
    generator.SetSeed(1);
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                // if (generator.NextFloat() < 0.3)
                // {
                //     continue;
                // }
                int type = generator.NextInt(0, 1);
                Vector3 pointPos = originPoint + Vector3(x + 0.5f, y + 0.5f, z + 0.5f) * UnitBlockSize;
                pointPos = Vector3(pointPos.GetX(), pointPos.GetZ(), pointPos.GetY());
                if (x != 0 || y != 0 || z != 0)
                {
                    type = BlockResourceManager::BlockType::Wood;
                }
                else
                {
                    type = BlockResourceManager::BlockType::TBall;
                }
                blocks[x][y][z] = Block(pointPos, BlockResourceManager::BlockType(type), UnitBlockRadius);
                blocks[x][y][z].model.Resize(World::UnitBlockRadius);
                blocks[x][y][z].model.Translate(pointPos);
            }
        }
    }
}

void WorldBlock::InitBlocks()
{
    RandomlyGenerateBlocks();
    SearchBlocksAdjacent2OuterAir();
    InitOcclusionQueriesHeaps();
}

void WorldBlock::Update(GraphicsContext& gfxContext, float deltaTime)
{
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                blocks[x][y][z].Update(gfxContext, deltaTime);
            }
        }
    }
}

int WorldBlock::GetBlockOffsetOnHeap(int x, int y, int z) const
{
    return z * (worldBlockSize * worldBlockSize) + y * worldBlockSize + x;
}

void WorldBlock::SearchBlocksAdjacent2OuterAir()
{
    std::vector<std::vector<std::vector<int>>> blocksStatus(worldBlockSize,
                                                            std::vector<std::vector<int>>(worldBlockSize,
                                                                std::vector<int>(worldBlockDepth)));

    // Assign outermost blocks as adjacent to outer air.
    // Outermost blocks has a same attribute, one of x,y equals 0 or worldBlockSize-1
    //                                  or z equals 0 or worldBlockDepth-1;
    for (int y = 0; y < worldBlockSize; y++)
    {
        for (int z = 0; z < worldBlockDepth; z++)
        {
            blocks[0][y][z].adjacent2OuterAir = true;
            blocks[worldBlockSize - 1][y][z].adjacent2OuterAir = true;
        }
    }

    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int z = 0; z < worldBlockDepth; z++)
        {
            blocks[x][0][z].adjacent2OuterAir = true;
            blocks[x][worldBlockSize - 1][z].adjacent2OuterAir = true;
        }
    }

    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            blocks[x][y][0].adjacent2OuterAir = true;
            blocks[x][y][worldBlockDepth - 1].adjacent2OuterAir = true;
        }
    }
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                if (blocks[x][y][z].IsNull() && blocks[x][y][z].adjacent2OuterAir)
                {
                    SpreadAdjacent2OuterAir(x, y, z, blocksStatus);
                }
            }
        }
    }
}

bool WorldBlock::CheckOutOfRange(int x, int y, int z) const
{
    return x < 0 || x >= worldBlockSize || y < 0 || y >= worldBlockSize || z < 0 || z >= worldBlockDepth;
}

void WorldBlock::RenderSingleBlock(int x, int y, int z, Renderer::MeshSorter& sorter)
{
    blocksRenderedVector.push_back(Point(x, y, z));
    if (EnableOcclusion)
    {
        if (!GetUnitBlockOcclusionResultFromVector(x, y, z)
            || !GetUnitBlockOcclusionResultFromVector(x + 1, y, z)
            || !GetUnitBlockOcclusionResultFromVector(x - 1, y, z)
            || !GetUnitBlockOcclusionResultFromVector(x, y + 1, z)
            || !GetUnitBlockOcclusionResultFromVector(x, y - 1, z)
            || !GetUnitBlockOcclusionResultFromVector(x, y, z + 1)
            || !GetUnitBlockOcclusionResultFromVector(x, y, z - 1))
        {
            count++;
            blocks[x][y][z].Render(sorter);
        }
    }
}

void WorldBlock::GetAllBlockOcclusionResult()
{
    blocksOcclusionState.resize(worldBlockSize,
                                std::vector<std::vector<bool>>(worldBlockSize,
                                                               std::vector<bool>(worldBlockDepth, true)));

    for (auto point : blocksRenderedVector)
    {
        int x = point.x;
        int y = point.y;
        int z = point.z;
        blocksOcclusionState[x][y][z] = GetUnitBlockOcclusionResult(x, y, z);
    }
}

bool WorldBlock::GetUnitBlockOcclusionResult(int x, int y, int z) const
{
    int offset = GetBlockOffsetOnHeap(x, y, z);
    size_t sizeOffset = offset * sizeof(uint64_t);
    uint64_t* CheckMemory = nullptr;
    m_queryReadBackBuffer->Map(0,
                               &CD3DX12_RANGE(sizeOffset, sizeOffset + sizeof(uint64_t)),
                               reinterpret_cast<void**>(&CheckMemory));
    return *(CheckMemory + offset) == 0 ? true : false;
}

bool WorldBlock::GetUnitBlockOcclusionResultFromVector(int x, int y, int z) const
{
    if (CheckOutOfRange(x,y,z))
    {
        return true;
    }
    return blocksOcclusionState[x][y][z];
}

void WorldBlock::CheckOcclusion(Renderer::MeshSorter& sorter, GraphicsContext& context, GlobalConstants& globals)
{
    context.GetCommandList()->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult, D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                 D3D12_RESOURCE_STATE_COPY_DEST));
    for (auto point : blocksRenderedVector)
    {
        sorter.ClearMeshes();
        int x = point.x;
        int y = point.y;
        int z = point.z;

        context.BeginQuery(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, GetBlockOffsetOnHeap(x, y, z));

        blocks[x][y][z].Render(sorter);
        sorter.Sort();
        sorter.RenderMeshesForOcclusion(Renderer::MeshSorter::kOpaque, context, globals);
        context.EndQuery(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, GetBlockOffsetOnHeap(x, y, z));
        context.GetCommandList()->ResolveQueryData(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION,
                                                   GetBlockOffsetOnHeap(x, y, z), 1, m_queryResult,
                                                   GetBlockOffsetOnHeap(x, y, z) * 8);
    }
    context.GetCommandList()->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult, D3D12_RESOURCE_STATE_COPY_DEST,
                                                 D3D12_RESOURCE_STATE_COPY_SOURCE));
}

void WorldBlock::RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                                     Renderer::MeshSorter& sorter, const Camera& camera)
{
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                Block& block = blocks[x][y][z];

                if (!block.IsNull()
                    && block.adjacent2OuterAir
                    && camera.GetWorldSpaceFrustum().IntersectBoundingBox(block.model.GetAxisAlignedBox()))
                {
                    RenderSingleBlock(x, y, z, sorter);
                }
            }
        }
    }
}

void WorldBlock::RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                                                     Renderer::MeshSorter& sorter, const Camera& camera)
{
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                Block& block = blocks[x][y][z];

                if (!block.IsNull()
                    && block.adjacent2OuterAir)
                {
                    RenderSingleBlock(x, y, z, sorter);
                }
            }
        }
    }
}


void WorldBlock::OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
                                    Renderer::MeshSorter& sorter, const Camera& camera)
{
    //check bounding box intersection
    Block& block1 = blocks[minX][minY][minZ];
    Block& block2 = blocks[minX][maxY][minZ];
    Block& block3 = blocks[minX][minY][maxZ];
    Block& block4 = blocks[minX][maxY][maxZ];
    Block& block5 = blocks[maxX][minY][minZ];
    Block& block6 = blocks[maxX][maxY][minZ];
    Block& block7 = blocks[maxX][minY][maxZ];
    Block& block8 = blocks[maxX][maxY][maxZ];

    AxisAlignedBox box;
    box.AddBoundingBox(block1.model.GetAxisAlignedBox());
    box.AddBoundingBox(block2.model.GetAxisAlignedBox());
    box.AddBoundingBox(block3.model.GetAxisAlignedBox());
    box.AddBoundingBox(block4.model.GetAxisAlignedBox());
    box.AddBoundingBox(block5.model.GetAxisAlignedBox());
    box.AddBoundingBox(block6.model.GetAxisAlignedBox());
    box.AddBoundingBox(block7.model.GetAxisAlignedBox());
    box.AddBoundingBox(block8.model.GetAxisAlignedBox());

    if (!camera.GetWorldSpaceFrustum().IntersectBoundingBox(box))
    {
        return;
    }

    if (EnableContainTest && camera.GetWorldSpaceFrustum().ContainingBoundingBox(box))
    {
        RenderBlocksInRangeNoIntersectCheck(minX, maxX, minY, maxY, minZ, maxZ, sorter, camera);
        return;
    }
    // if depth > n, then invoke render in range
    // if any node side reaches 1, invoke render in range
    if (depth >= MaxOctreeDepth || maxX - minX <= MaxOctreeNodeLength || maxY - minY <= MaxOctreeNodeLength || maxZ -
        minZ <= MaxOctreeNodeLength)
    {
        RenderBlocksInRange(minX, maxX, minY, maxY, minZ, maxZ, sorter, camera);
        return;
    }

    // separate to eight sub space.
    int middleX = (maxX - minX) / 2 + minX;
    int middleY = (maxY - minY) / 2 + minY;
    int middleZ = (maxZ - minZ) / 2 + minZ;

    OctreeRenderBlocks(minX, middleX, minY, middleY, minZ, middleZ, depth + 1, sorter, camera);
    OctreeRenderBlocks(minX, middleX, minY, middleY, middleZ + 1, maxZ, depth + 1, sorter, camera);
    OctreeRenderBlocks(middleX + 1, maxX, minY, middleY, minZ, middleZ, depth + 1, sorter, camera);
    OctreeRenderBlocks(middleX + 1, maxX, minY, middleY, middleZ + 1, maxZ, depth + 1, sorter, camera);
    OctreeRenderBlocks(minX, middleX, middleY + 1, maxY, minZ, middleZ, depth + 1, sorter, camera);
    OctreeRenderBlocks(minX, middleX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1, sorter, camera);
    OctreeRenderBlocks(middleX + 1, maxX, middleY + 1, maxY, minZ, middleZ, depth + 1, sorter, camera);
    OctreeRenderBlocks(middleX + 1, maxX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1, sorter, camera);
}

void WorldBlock::CopyOnReadBackBuffer(GraphicsContext& context)
{
    context.GetCommandList()->CopyResource(m_queryReadBackBuffer, m_queryResult);
}

void WorldBlock::Render(Renderer::MeshSorter& sorter, const Camera& camera, GraphicsContext& context)
{
    CopyOnReadBackBuffer(context);
    GetAllBlockOcclusionResult();
    blocksRenderedVector.clear();
    count = 0;
    if (EnableOctree)
    {
        OctreeRenderBlocks(0, worldBlockSize - 1, 0, worldBlockSize - 1, 0, worldBlockDepth - 1, 0, sorter, camera);
    }
    else
    {
        count = 0;
        for (int x = 0; x < worldBlockSize; x++)
        {
            for (int y = 0; y < worldBlockSize; y++)
            {
                for (int z = 0; z < worldBlockDepth; z++)
                {
                    Block& block = blocks[x][y][z];

                    if (!block.IsNull()
                        && block.adjacent2OuterAir
                        && camera.GetWorldSpaceFrustum().IntersectBoundingBox(block.model.GetAxisAlignedBox()))
                    {
                        RenderSingleBlock(x, y, z, sorter);
                    }
                }
            }
        }
    }
    std::cout << "render count: " << count << std::endl;
}

void WorldBlock::CleanUp()
{
    for (auto blocksX : blocks)
    {
        for (auto blocksY : blocksX)
        {
            for (auto block : blocksY)
            {
                block.CleanUp();
            }
        }
    }
}

void WorldBlock::SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>>& blockStatus)
{
    // out of boundary
    if (x < 0 || x > worldBlockSize - 1 || y < 0 || y > worldBlockSize - 1 || z < 0 || z > worldBlockDepth - 1)
    {
        return;
    }

    blocks[x][y][z].adjacent2OuterAir = true;

    if (blockStatus[x][y][z])
    {
        return;
    }

    blockStatus[x][y][z] = 1;

    if (blocks[x][y][z].IsNull() || blocks[x][y][z].isTransparent())
    {
        SpreadAdjacent2OuterAir(x + 1, y, z, blockStatus);
        SpreadAdjacent2OuterAir(x - 1, y, z, blockStatus);
        SpreadAdjacent2OuterAir(x, y + 1, z, blockStatus);
        SpreadAdjacent2OuterAir(x, y - 1, z, blockStatus);
        SpreadAdjacent2OuterAir(x, y, z + 1, blockStatus);
        SpreadAdjacent2OuterAir(x, y, z - 1, blockStatus);
    }
}
