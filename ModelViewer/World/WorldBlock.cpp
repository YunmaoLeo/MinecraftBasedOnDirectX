#include "WorldBlock.h"

#include <stdbool.h>

#include "BufferManager.h"
#include "Renderer.h"
#include "ShadowCamera.h"
#include "SimplexNoise.h"
#include "World.h"
#include "Math/PerlinNoise.h"
#include "Math/Random.h"
using namespace Math;
using namespace World;

NumVar MaxOctreeDepth("Octree/Depth", 2, 0, 10, 1);
NumVar MaxOctreeNodeLength("Octree/Length", 2, 0, 20, 1);
BoolVar EnableOctree("Octree/EnableOctree", true);
BoolVar EnableContainTest("Octree/EnableContainTest", true);
BoolVar EnableOctreeCompute("Octree/ComputeOptimize", false);

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

    auto depthBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Graphics::g_SceneDepthBuffer.GetWidth() * Graphics::g_SceneColorBuffer.GetHeight() * sizeof(float));
    
    Graphics::g_Device->CreateCommittedResource(
        &readBackHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_depthReadBackBuffer));
}

void WorldBlock::CopyDepthBuffer(GraphicsContext& context)
{
    context.GetCommandList()->CopyResource(m_depthReadBackBuffer, Graphics::g_SceneDepthBuffer.GetResource());
}

void WorldBlock::ReadDepthBuffer(GraphicsContext& context)
{
    uint32_t width = Graphics::g_SceneDepthBuffer.GetWidth();
    uint32_t height = Graphics::g_SceneDepthBuffer.GetHeight();
    float* memory;
    HRESULT result = m_depthReadBackBuffer->Map(0,
        &CD3DX12_RANGE(0,width * height * 4),
        reinterpret_cast<void**>(&memory));
    printf("depthReadResult: %x \n", result);
    for (uint32_t x=0; x<width; x++)
    {
        for (uint32_t y=0; y<height; y++)
        {
            uint32_t offset = (y * width + x);
            if (*(memory+offset)!=0.0f)
            {
                std::cout << "depthRead Not null in x: "<<x<<" y: "<<y<<" with value: " << *(memory+offset)<<std::endl;
            }
        }
    }
}

void WorldBlock::RandomlyGenerateBlocks()
{
    //PerlinNoise noise = PerlinNoise(9);
    SimplexNoise noise(0.8,1,0.5,0.7);
    RandomNumberGenerator generator;
    generator.SetSeed(1);
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            //float(originPoint.GetX())+(x+0.5f)*UnitBlockSize*1.001,float(originPoint.GetY())+(y+0.5f)*UnitBlockSize*1.001, 0.8
            //double height = noise.noise((float(originPoint.GetX())+(x+0.5f)*UnitBlockSize*1.001)*0.001,(float(originPoint.GetY())+(y+0.5f)*UnitBlockSize*1.001)*0.001, 0.8);
            double height = noise.noise((float(originPoint.GetX())+(x+0.5f)*UnitBlockSize*1.001)*0.0005,(float(originPoint.GetY())+(y+0.5f)*UnitBlockSize*1.001)*0.0005);
            int realHeight = this->worldBlockDepth * (height+1)/2;
            if (realHeight >= this->worldBlockDepth) realHeight = this->worldBlockDepth-1;
            if (realHeight <0) realHeight = 0;
            for (int z = 0; z < realHeight; z++)
            {
                BlockType type;
                if (z == realHeight-1)
                {
                    type = Dirt;
                }
                else if (float(z)/float(realHeight) < 0.5 + generator.NextInt(-1,1)*0.15)
                {
                    type = Stone;
                }
                else if (float(z)/float(realHeight) < 1)
                {
                    type = Dirt;
                }
                Vector3 pointPos = originPoint + Vector3(x + 0.5f, y + 0.5f, z + 0.5f) * UnitBlockSize*1.001;
                pointPos = Vector3(pointPos.GetX(), pointPos.GetZ(), pointPos.GetY());
                blocks[x][y][z] = Block(pointPos, BlockResourceManager::BlockType(type), UnitBlockRadius);
            }
        }
    }
}

void WorldBlock::InitBlocks()
{
    RandomlyGenerateBlocks();
    SearchBlocksAdjacent2OuterAir();
    // InitOcclusionQueriesHeaps();
    CreateOctreeNode(this->octreeNode, 0, this->worldBlockSize-1, 0, this->worldBlockSize-1, 0, this->worldBlockDepth-1, 0);
}

void WorldBlock::Update(float deltaTime)
{
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = 0; z < worldBlockDepth; z++)
            {
                blocks[x][y][z].Update(deltaTime);
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
    // for (int y = 0; y < worldBlockSize; y++)
    // {
    //     for (int z = 0; z < worldBlockDepth; z++)
    //     {
    //         blocks[0][y][z].adjacent2OuterAir = true;
    //         blocks[worldBlockSize - 1][y][z].adjacent2OuterAir = true;
    //     }
    // }
    //
    // for (int x = 0; x < worldBlockSize; x++)
    // {
    //     for (int z = 0; z < worldBlockDepth; z++)
    //     {
    //         blocks[x][0][z].adjacent2OuterAir = true;
    //         blocks[x][worldBlockSize - 1][z].adjacent2OuterAir = true;
    //     }
    // }

    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            // blocks[x][y][0].adjacent2OuterAir = true;
            blocks[x][y][worldBlockDepth - 1].adjacent2OuterAir = true;
        }
    }
    for (int x = 0; x < worldBlockSize; x++)
    {
        for (int y = 0; y < worldBlockSize; y++)
        {
            for (int z = worldBlockDepth-1; z >= 0; z--)
            {
                if (blocks[x][y][z].IsNull() && blocks[x][y][z].adjacent2OuterAir)
                {
                    SpreadAdjacent2OuterAir(x + 1, y, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x - 1, y, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y + 1, z, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y - 1, z, blocksStatus);
                    // SpreadAdjacent2OuterAir(x, y, z + 1, blocksStatus);
                    SpreadAdjacent2OuterAir(x, y, z - 1, blocksStatus);
                }
            }
        }
    }
}

bool WorldBlock::CheckOutOfRange(int x, int y, int z) const
{
    return x < 0 || x >= worldBlockSize || y < 0 || y >= worldBlockSize || z < 0 || z >= worldBlockDepth;
}

void WorldBlock::RenderSingleBlock(int x, int y, int z)
{
    // blocksRenderedVector.push_back(Point(x, y, z));
    if (Renderer::EnableOcclusion)
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
            Block& block = blocks[x][y][z];
            BlockResourceManager::addBlockIntoManager(block.blockType, block.position, block.sideSize);
        }
    }
    else
    {
        count++;
        Block& block = blocks[x][y][z];
        BlockResourceManager::addBlockIntoManager(block.blockType, block.position, block.sideSize);
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
    std::cout << "checkOcclusionCount: "<<blocksRenderedVector.size()<<std::endl; 
    context.GetCommandList()->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult, D3D12_RESOURCE_STATE_PREDICATION,
                                                 D3D12_RESOURCE_STATE_COPY_DEST));
    for (auto point : blocksRenderedVector)
    {
        sorter.ClearMeshes();
        int x = point.x;
        int y = point.y;
        int z = point.z;

        context.BeginQuery(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, GetBlockOffsetOnHeap(x, y, z));

        // blocks[x][y][z].Render(sorter);
        sorter.Sort();
        sorter.RenderMeshesForOcclusion(Renderer::MeshSorter::kZPass, context, globals);
        context.EndQuery(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, GetBlockOffsetOnHeap(x, y, z));
    }

    for (auto point:blocksRenderedVector)
    {
        int x= point.x;
        int y = point.y;
        int z = point.z;
        context.GetCommandList()->ResolveQueryData(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION,
                                           GetBlockOffsetOnHeap(x, y, z), 1, m_queryResult,
                                           GetBlockOffsetOnHeap(x, y, z) * 8);
    }
    context.GetCommandList()->ResourceBarrier(
        1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult, D3D12_RESOURCE_STATE_COPY_DEST,
                                                 D3D12_RESOURCE_STATE_PREDICATION));
}

void WorldBlock::RenderBlocksInRange(int minX, int maxX, int minY, int maxY, int minZ, int maxZ,
                                     const Camera& camera)
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
                    && camera.GetWorldSpaceFrustum().IntersectSphere(block.boundingSphere)
                    )
                {
                    RenderSingleBlock(x, y, z);
                }
            }
        }
    }
}

void WorldBlock::RenderBlocksInRangeNoIntersectCheck(int minX, int maxX, int minY, int maxY, int minZ, int maxZ)
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
                    RenderSingleBlock(x, y, z);
                }
            }
        }
    }
}

void WorldBlock::CreateOctreeNode(OctreeNode* &node, int minX,int maxX, int minY, int maxY, int minZ, int maxZ, int depth)
{
    if (minX >= maxX || minY >= maxY || minZ >= maxZ)
    {
        return;
    }
    int maxDepth = 3;
    bool isLeafNode = depth == maxDepth ? true : false;
    node = new OctreeNode(minX, maxX, minY, maxY, minZ, maxZ, isLeafNode);

    AxisAlignedBox box;

    for (int x=minX; x<=maxX;x+=(maxX-minX))
    {
        for (int y = minY; y<=maxY; y+=(maxY-minY))
        {
            for (int z = minZ; z<=maxZ; z+=(maxZ-minZ))
            {
                Vector3 worldPoint = {originPoint.GetX(), originPoint.GetZ(), originPoint.GetY()};
                Vector3 minPoint = worldPoint + (Vector3(x,z,y) * UnitBlockSize);
                Vector3 maxPoint = worldPoint + (Vector3(x+1,z+1,y+1) * UnitBlockSize);
                box = std::move(box.Union({minPoint, maxPoint}));
            }
        }
    }
    node->box = std::move(box);

    if (depth==maxDepth)
    {
        return;
    }

    // separate to eight sub space.
    int middleX = (maxX - minX) / 2 + minX;
    int middleY = (maxY - minY) / 2 + minY;
    int middleZ = (maxZ - minZ) / 2 + minZ;

    CreateOctreeNode(node->leftBottomBack,minX, middleX, minY, middleY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->leftTopBack,minX, middleX, minY, middleY, middleZ + 1, maxZ, depth + 1);
    CreateOctreeNode(node->rightBottomBack,middleX + 1, maxX, minY, middleY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->rightTopBack,middleX + 1, maxX, minY, middleY, middleZ + 1, maxZ, depth + 1);
    CreateOctreeNode(node->leftBottomFront,minX, middleX, middleY + 1, maxY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->leftTopFront,minX, middleX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1);
    CreateOctreeNode(node->rightBottomFront,middleX + 1, maxX, middleY + 1, maxY, minZ, middleZ, depth + 1);
    CreateOctreeNode(node->rightTopFront,middleX + 1, maxX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1);
    
}

void WorldBlock::OctreeRenderBlocks(OctreeNode* &node, const Camera& camera)
{
    AxisAlignedBox box = node->box;

    if (!camera.GetWorldSpaceFrustum().IntersectBoundingBox(box))
    {
        return;
    }

    int minX = node->minX;
    int maxX = node->maxX;
    int minY = node->minY;
    int maxY = node->maxY;
    int minZ = node->minZ;
    int maxZ = node->maxZ;

    if (EnableContainTest && camera.GetWorldSpaceFrustum().ContainingBoundingBox(box))
    {
        std::cout << "pass contain test" << std::endl;
        RenderBlocksInRangeNoIntersectCheck(minX, maxX, minY, maxY, minZ, maxZ);
        return;
    }
    // if depth > n, then invoke render in range
    // if any node side reaches 1, invoke render in range
    if (node->isLeafNode|| maxX - minX <= MaxOctreeNodeLength || maxY - minY <= MaxOctreeNodeLength || maxZ -
        minZ <= MaxOctreeNodeLength)
    {
        RenderBlocksInRange(minX, maxX, minY, maxY, minZ, maxZ, camera);
        return;
    }

    // separate to eight sub space.
    OctreeRenderBlocks(node->leftBottomBack, camera);
    OctreeRenderBlocks(node->leftBottomFront, camera);
    OctreeRenderBlocks(node->leftTopBack, camera);
    OctreeRenderBlocks(node->leftTopFront, camera);
    OctreeRenderBlocks(node->rightBottomBack, camera);
    OctreeRenderBlocks(node->rightBottomFront, camera);
    OctreeRenderBlocks(node->rightTopBack, camera);
    OctreeRenderBlocks(node->rightTopFront, camera);
}

void WorldBlock::OctreeRenderBlocks(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, int depth,
                                    const Camera& camera)
{
    //check bounding box intersection
    AxisAlignedBox box;
    
    Block& block1 = blocks[minX][minY][minZ];
    Block& block2 = blocks[minX][maxY][minZ];
    Block& block3 = blocks[minX][minY][maxZ];
    Block& block4 = blocks[minX][maxY][maxZ];
    Block& block5 = blocks[maxX][minY][minZ];
    Block& block6 = blocks[maxX][maxY][minZ];
    Block& block7 = blocks[maxX][minY][maxZ];
    Block& block8 = blocks[maxX][maxY][maxZ];


    box.AddBoundingBox(block1.axisAlignedBox);
    box.AddBoundingBox(block2.axisAlignedBox);
    box.AddBoundingBox(block3.axisAlignedBox);
    box.AddBoundingBox(block4.axisAlignedBox);
    box.AddBoundingBox(block5.axisAlignedBox);
    box.AddBoundingBox(block6.axisAlignedBox);
    box.AddBoundingBox(block7.axisAlignedBox);
    box.AddBoundingBox(block8.axisAlignedBox);
    
    if (!camera.GetWorldSpaceFrustum().IntersectBoundingBox(box))
    {
        return;
    }

    if (EnableContainTest && camera.GetWorldSpaceFrustum().ContainingBoundingBox(box))
    {
        std::cout << "pass contain test" << std::endl;
        RenderBlocksInRangeNoIntersectCheck(minX, maxX, minY, maxY, minZ, maxZ);
        return;
    }
    // if depth > n, then invoke render in range
    // if any node side reaches 1, invoke render in range
    if (depth >= MaxOctreeDepth || maxX - minX <= MaxOctreeNodeLength || maxY - minY <= MaxOctreeNodeLength || maxZ -
        minZ <= MaxOctreeNodeLength)
    {
        RenderBlocksInRange(minX, maxX, minY, maxY, minZ, maxZ, camera);
        return;
    }

    // separate to eight sub space.
    int middleX = (maxX - minX) / 2 + minX;
    int middleY = (maxY - minY) / 2 + minY;
    int middleZ = (maxZ - minZ) / 2 + minZ;

    OctreeRenderBlocks(minX, middleX, minY, middleY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(minX, middleX, minY, middleY, middleZ + 1, maxZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, minY, middleY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, minY, middleY, middleZ + 1, maxZ, depth + 1, camera);
    OctreeRenderBlocks(minX, middleX, middleY + 1, maxY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(minX, middleX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, middleY + 1, maxY, minZ, middleZ, depth + 1, camera);
    OctreeRenderBlocks(middleX + 1, maxX, middleY + 1, maxY, middleZ + 1, maxZ, depth + 1, camera);
}

void WorldBlock::CopyOnReadBackBuffer(GraphicsContext& context)
{
    context.GetCommandList()->CopyResource(m_queryReadBackBuffer, m_queryResult);
}

bool WorldBlock::Render(const Camera& camera, GraphicsContext& context)
{
    if (Renderer::EnableOcclusion)
    {
        CopyOnReadBackBuffer(context);
        GetAllBlockOcclusionResult();
    }

    // blocksRenderedVector.clear();
    count = 0;
    if (EnableOctree)
    {
        if (EnableOctreeCompute)
        {
            OctreeRenderBlocks(this->octreeNode, camera);
        }
        else
        {
            OctreeRenderBlocks(0, worldBlockSize - 1, 0, worldBlockSize - 1, 0, worldBlockDepth - 1, 0, camera);
        }
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
                        //&& camera.GetWorldSpaceFrustum().IntersectBoundingBox(block.axisAlignedBox)
                        )
                    {
                        RenderSingleBlock(x, y, z);
                    }
                }
            }
        }
    }
    std::cout << "render count: " << count << " blockID: " << this->id<< std::endl;
    return false;
}

void WorldBlock::CleanUp()
{
    for (auto blocksX : blocks)
    {
        for (auto blocksY : blocksX)
        {
            for (auto block : blocksY)
            {
                block.ClearUp();
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

    if (blockStatus[x][y][z])
    {
        return;
    }
    
    blocks[x][y][z].adjacent2OuterAir = true;
    

    blockStatus[x][y][z] = 1;
    //
    // if (blocks[x][y][z].IsNull() || blocks[x][y][z].isTransparent())
    // {
    //     SpreadAdjacent2OuterAir(x + 1, y, z, blockStatus);
    //     SpreadAdjacent2OuterAir(x - 1, y, z, blockStatus);
    //     SpreadAdjacent2OuterAir(x, y + 1, z, blockStatus);
    //     SpreadAdjacent2OuterAir(x, y - 1, z, blockStatus);
    //     // SpreadAdjacent2OuterAir(x, y, z + 1, blockStatus);
    //     SpreadAdjacent2OuterAir(x, y, z - 1, blockStatus);
    // }
}
