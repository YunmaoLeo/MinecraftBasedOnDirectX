Based on Microsoft:MiniEngine

IN PROGRESSING...

### Step 1.1
1. Create Block Resource Manager
2. Create and implement draft Block, WorldBlock class
_Performance declines when number of blocks reaches over 1000_

Program is now allow to generate a simple random world of blocks with scene like follows:
![image](https://user-images.githubusercontent.com/79432990/216819683-09fb40c5-a698-4c18-be8c-f886b14b8e44.png)


### Step 1.2
**Render Performance Optimization**
Now, we focus on how to minimize the render cost.

First, illustrate the time cost of each step with two different camera perspective.

![Untitled](https://s3-us-west-2.amazonaws.com/secure.notion-static.com/b7b5e58d-a2bf-4983-a8cd-3bbd8796e09b/Untitled.png)

1. Looking at the whole blocks.

CPU cost/frame: 50.3ms

Blocks Render to Sorter: 31706

Depth Pre-Pass: 182192 ticks

Sun Shadow Map: 235159 ticks

Render Color: 198821 ticks.

![Untitled](https://s3-us-west-2.amazonaws.com/secure.notion-static.com/30263561-90a9-4677-8ef3-c73f71580f2c/Untitled.png)

1. Looking at a corner of blocks.

CPU cost/frame: 7.4ms

Block Render to sorter: 30267

depth pre-pass: 5096

sun shadow map: 35377

render color: 5201

**The main different of these two camera perspective is the number of blocks that contained in each frustum.**

```cpp
// Main Steps of Rendering Each Frame
MeshSorter sorter(); // a. create a sorter that responsible for drawing every mesh.

blocks.Render(sorter); // b. add blocks' meshes into sorter.

sorter.renderMeshes(); // c. render meshes.
```

To optimize render step, the main approach is to minimize the blocks that need CPU to compute. For instance:

1. **Minimize the blocks that add into the mesh sorter.**
    
    In origin implementation, we simply invoke each blocks’ Render(), add their meshes to sorter, which brings large overcost.
    
    ```cpp
    void WorldBlock::Render(Renderer::MeshSorter& sorter)
    {
        for (int x = 0; x < worldBlockSize; x++)
        {
            for (int y = 0; y < worldBlockSize; y++)
            {
                for (int z =0; z < worldBlockDepth; z++)
                {
                    blocks[x][y][z].Render(sorter);}}}}
    ```
    
    We trying to adopt following optimize strategies.
    
    1. **Those blocks that been occluded are unable to seen, which we don’t need to  render them.**
        
        ![Untitled](https://s3-us-west-2.amazonaws.com/secure.notion-static.com/ac4ca78c-0279-4cc5-a886-eda32ccee741/Untitled.png)
        
        Considering above blocks, blue means there is a blick, white means there is air. Only blocks  adjacent to **outter air** can be able to seen. (However, we leave transparent blocks aside for now.)
        
        ### Implementation of Adjacent to Air Block Detection
        
        1. First, we consider all the blocks in the outermost layer are visible.
        2. Starting from the outtermost layer, we spread the attribute **adjacent to outter air** by use of Depth First Search.
        
        ```cpp
        void WorldBlock::SearchBlocksAdjacent2OuterAir()
        {
            std::vector<std::vector<std::vector<int>>> blocksStatus(worldBlockSize,
                std::vector<std::vector<int>>(worldBlockSize,
                    std::vector<int>(worldBlockDepth)));
        
            // Assign outermost blocks as adjacent to outer air.
            // Outermost blocks has a same attribute, one of x,y equals 0 or worldBlockSize-1
            //                                  or z equals 0 or worldBlockDepth-1;
            for (int y=0; y<worldBlockSize; y++)
            {
                for (int z=0; z<worldBlockDepth; z++)
                {
                    blocks[0][y][z].adjacent2OuterAir = true;
                    blocks[worldBlockSize-1][y][z].adjacent2OuterAir = true;
                }
            }
        
            for (int x=0; x<worldBlockSize; x++)
            {
                for (int z=0; z<worldBlockDepth; z++)
                {
                    blocks[x][0][z].adjacent2OuterAir = true;
                    blocks[x][worldBlockSize-1][z].adjacent2OuterAir = true;
                }
            }
        
            for (int x=0; x<worldBlockSize; x++)
            {
                for (int y=0; y<worldBlockSize; y++)
                {
                    blocks[x][y][0].adjacent2OuterAir = true;
                    blocks[x][y][worldBlockDepth-1].adjacent2OuterAir = true;
                }
            }
            for (int x = 0; x < worldBlockSize; x++)
            {
                for (int y = 0; y < worldBlockSize; y++)
                {
                    for (int z =0; z < worldBlockDepth; z++)
                    {
                        if (blocks[x][y][z].IsNull() && blocks[x][y][z].adjacent2OuterAir)
                        {
                            SpreadAdjacent2OuterAir(x,y,z,blocksStatus);
                        }
                    }
                }
            }
            
        }
        
        void WorldBlock::SpreadAdjacent2OuterAir(int x, int y, int z, std::vector<std::vector<std::vector<int>>> &blockStatus)
        {
            // out of boundary
            if (x < 0 || x > worldBlockSize-1 || y<0 || y>worldBlockSize-1 || z<0 || z>worldBlockDepth-1)
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
                SpreadAdjacent2OuterAir(x+1,y,z, blockStatus);
                SpreadAdjacent2OuterAir(x-1,y,z, blockStatus);
                SpreadAdjacent2OuterAir(x,y+1,z, blockStatus);
                SpreadAdjacent2OuterAir(x,y-1,z, blockStatus);
                SpreadAdjacent2OuterAir(x,y,z+1, blockStatus);
                SpreadAdjacent2OuterAir(x,y,z-1, blockStatus);
            }
        
        }
        ```
        
        Before Adopting this:
        
        - Average CPU time: 50~51ms
        - **Number of Blocks that rendered: 4500 (optimize target)**
        - Blocks Render To Sorter: 48837
        - RenderMeshes: 195511
        
        After Adopting this:
        
        - Average CPU time: 44~45ms
        - **Number of Blocks that rendered: 3847 (where we optimize)**
        - Blocks Render To Sorter: 40491
        - RenderMeshes: 164427
        
        **********Conclusion: Blocks that are not able to see is banned. Thus, performance has been increased accordingly.**********
        
    
    b. **In addition, we adopt frustum culling. Those blocks that not in a frustum should be culled.**
    
    1. Frustum Culling can be adopted by adding a bounding box detection.
        
        Use octree to accelerate the speed.  (15*15*15) 2859 blocks
        
        - Before Using Octree
            
            Detect and render meshes into the sorter:
            
            - nothing in view frustum: 20000ticks
            - full scene in view frustum: 85000 ticks (O(n))
            - half scene in view frustum: 63096 ticks (1687blocks) (22.73ticks/blocsk)
                
                ![Untitled](https://s3-us-west-2.amazonaws.com/secure.notion-static.com/d1036b76-9c76-4694-b60b-3d3d221d9120/Untitled.png)
                
        - After Using Octree
            
            Detect and render meshes into the sorter:
            
            - nothing in view frustum: 750ticks
            - full scene in view frustum: 88000 ticks (O(n))
            - half scene in view frustum: 71078 ticks (1670)
    
    ## Occlusion Culling Implementation
    
    [https://www.gamesindustry.biz/overview-on-popular-occlusion-culling-techniques](https://www.gamesindustry.biz/overview-on-popular-occlusion-culling-techniques)
    
    [Coverage Buffer as main Occlusion Culling technique](https://www.gamedev.net/tutorials/_/technical/graphics-programming-and-theory/coverage-buffer-as-main-occlusion-culling-technique-r4103/)
    
    Use the depth buffer from last frame and reproject it to the current frame.
    
    [Queries - Win32 apps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/queries#query-heaps)
    
    [Predication queries - Win32 apps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/predication-queries)
    
    Here I use GPU occlusion culling (zbuffer test from last frame)
    
    ```cpp
    //Create Occlusion Query Heap, Result.
    
    // Before drawing a thing, use the occlusion result of last frame to set the predication(whether to command draws)
    context.setPredication(queryResult.get(),...);
    context.draw();
    
    // 
    BeginQuery()
    contenxt.drawBoudingBox();
    EndQuery();
    
    ResolveQuery();
    ```
    
    Performance:
    
    The scene:
    
    ![Untitled](https://s3-us-west-2.amazonaws.com/secure.notion-static.com/dede0f2a-0319-46d8-a9b8-fac16a189de3/Untitled.png)
    
    ![Untitled](https://s3-us-west-2.amazonaws.com/secure.notion-static.com/a61752b9-3c73-4c66-8a13-1ae79b16bd82/Untitled.png)
    
    Occludee: chess board, with a large amount of meshes.
    
    We move our camera to back of a diamond block.
    
    - Before we turn on the Occlusion Culling
        - GPU will execute drawInstance, but will cull meshes while doing z-buffer.
        - GPU costs about 2.03ms for a frame
        
        ![Untitled](https://s3-us-west-2.amazonaws.com/secure.notion-static.com/d67d45e8-fab9-43a1-8e54-5ca10634f535/Untitled.png)
        
    - After:
        - GPU will not execute any command relative to drawInstance process
        - GPU costs about 1.43ms for a frame.
