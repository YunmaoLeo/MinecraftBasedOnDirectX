# Portfolio Link
Holistic description of this project on my portfolio website: https://leolejie.notion.site/Game-Project-Portfolio-by-Lejie-LIU-5cc44e10b56c4a62917edf23ada6e7f9?pvs=4
## 1. Effect Preview Based on the Methods in This Article:
+ Plains
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/944ba9f3b1c641248ff8ff8ec5f4029d~tplv-k3u1fbpfcp-watermark.image?=512x384)
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f04ce974460b4d838f4903e2253f1cc7~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ Inland Mountains
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3f771a5bfaa84cdf84fb9ac64ff15112~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ Desert
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/20f7c2ceec924d25afbe271e6302bf7e~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ Tropical Jungle
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/321d06bc8e0a4fafa11d26a8db98d29e~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ Snow & Snow Mountains
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/51a6ce32050a49acbcfc3297e9a611fe~tplv-k3u1fbpfcp-watermark.image?=512x384)
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0b10e5e1d1ee4652bb7b47045babbd5b~tplv-k3u1fbpfcp-watermark.image?=512x384)

## 2. Method Overview
We generate map height maps based on three sets of Simplex Noise (base elevation, terrain erosion, peak-valley fluctuation) and corresponding linear interpolation, combined with temperature and humidity maps generated using noise to determine biome characteristics (vegetation density, surface type), generating various combinations of terrain and landscapes.

# Implementation
## 1. Map Generation Step Breakdown
For the basic map in a sandbox game, we divide it from bottom to top:
1. Terrain Height: This stage is the foundation of all steps. We randomly generate the height of each unit point to obtain basic terrain features such as lakes, plains, hills, mountains, plateaus, and valleys.
2. Climate: Based on different temperature and humidity levels, combined with terrain height and distance from the ocean, we obtain the climate composition of the world map.
3. Surface Terrain: We can divide different location terrains through climate and current topography, such as snow on snowy mountains, water on lakes, grassland on forests, and withered grassland on inland arid plateaus.
4. Underground Caves
5. Decoration Layer: Vegetation (trees, grass), animal groups, villages, and other object groups added based on existing terrain and climate.

Next, let's look at the implementation details of each layer step by step.
## 2. Random Terrain Height Generation
For generating random terrain height, we can naturally use random numbers.
### White Noise Terrain
+ Simply calling `random.nextInt(MAX_HEIGHT)` in a loop can generate white noise terrain (that is, the height between points is unrelated, all independent of each other), and we immediately get the following irregular terrain.
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b335a77371df410ba7d16c15b5ae140a~tplv-k3u1fbpfcp-watermark.image?=512x384)

### First Attempt: Generating Terrain Based on Simple Simplex Noise
+ Obviously, we need progressive randomness to provide us with a continuous map height. There are many widely used random noise algorithms in game random generation, and we just need to choose the appropriate one.
    + **Perlin Noise**: Applies gradients and interpolation based on random numbers, creating natural and continuous random effects between points.
    + **Simplex Noise**: An improved version of Perlin Noise that generates noise through equilateral polyhedra in high-dimensional space, then calculates results through projection and interpolation, with faster runtime efficiency and better performance. This is the noise we chose.
    + **Value Noise**: Grid-based noise that samples and interpolates within the grid to generate noise. Compared to Perlin Noise, it has weaker layers and slightly worse effects, but runs faster.
    + **Fractal Noise**: Generates a basic noise image, then performs multiple scaling, translation, and rotation operations on that image to stack and generate complex, natural noise images.

+ Core parameters of Simplex Noise
    + **Frequency**: The higher the frequency, the more frequent the fluctuations in the generated noise.
    + **Lacunarity**: Controls the overall shape in the noise. Higher lacunarity generates irregular shapes, while lower lacunarity generates smooth shapes.
    + **Amplitude**: Controls the amplification factor of the noise (vertical stretching or compression)

+ Terrain Generation
For the height generation of each unit point, we use the following method:
    SimplexNoise accepts a unit coordinate and outputs a value in the range (-1,1). We consider -1 to represent the lowest point, 1 to represent the highest point, and intermediate values are scaled proportionally.
```C++
//x, y are the coordinates of the point
float noise = SimplexNoise(x,y)
//Specify a minimum surface height for the world
int SURFACE_HEIGHT = 20;
int height = SURFACE_HEIGHT + (noise+1)/2 * (128); 
```
Our Simplex Noise has the following parameter settings:
+ Frequency: 0.3
+ Amplitude: 1
+ Lacunarity: 2

Using this SimplexNoise, we generated the following terrain. The regular fluctuations bring a feeling of rolling mountains, and the minimum SURFACE_HEIGHT we set also brings some basic river beds. Compared to white noise terrain, it has a better effect, but for a terrain, it is somewhat too regular and lacks variation.
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/33663525b0cf460ca0915f89d1e5770a~tplv-k3u1fbpfcp-watermark.image?=512x384)
When we reduce the SimplexNoise parameter Frequency to 0.1, we get terrain with larger mountains, smaller heights, and gentler fluctuations.
![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/afa539f7d9dd43e29c911586b37d1409~tplv-k3u1fbpfcp-watermark.image?=512x384)

### Setting Linear Interpolation to Achieve Variable Base Heights
+ Using a simple layer of SimplexNoise gives us a gradually changing terrain.
+ However, such terrain seems too monotonous, always being continuous fluctuations based on SURFACE_HEIGHT.
+ For real terrain, we want it to have multiple different base heights, such as:
    + Fluctuations based on SURFACE_HEIGHT form hills
    + Fluctuations based on SURFACE_HEIGHT+20 form mountains
    + Fluctuations based on SURFACE_HEIGHT+50 form plateaus

The following line chart shows how we linearly interpolate the output noise. We input noise within the range of (-1,1), interpolate according to defined points, and output height.
+ This is the linear interpolation we tried in the previous section. noise=-1 marks the lowest point, noise=1 marks the highest point
![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f086e33031384a84a532ff95d40c0008~tplv-k3u1fbpfcp-watermark.image?)
+ To give our terrain more levels, we try to set up interpolation according to sea level, plains, mountains, and plateaus, as shown below:
    + Positions where noise is below -0.6 are all set to sea level height
    + When noise is between -0.6 and -0.5, we consider it as a transition segment from sea to plain
    + When noise is between -0.5 and 0, it is set as plains with lower elevation
    + When noise is between 0 and 0.4, it is set as mountains with gradually increasing height
    + When noise is 0.5 and above, it is set as plateaus
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7c97101f667947feb4552dbdbc0907ea~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ Based on this interpolation, we obtained the following terrain effect, see image below

![1678198009840.jpg](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ee2fb019f13a444bbe7787002452e78b~tplv-k3u1fbpfcp-watermark.image?=512x384)
    + The closest to us, with the lowest elevation, is the ocean/lake
    + One level higher is the plain we set, vast and flat
    + Even higher and fluctuating is the mountain terrain we seek
    + The farthest and highest is the plateau
+ The **terrain effect produced by this linear interpolation has more levels**, but it is also filled with **an overly smooth and strange appearance**, such as the transition from lake to plain always being a slope, with no fluctuations on plateaus or plains.
### Combining Three Different Noise Sets
In the previous section, we tried using one set of noise and its linear interpolation to achieve rough effects of oceans, plains, mountains, and highlands. We might as well consider whether we can combine multiple sets of noise to merge them into a more natural terrain.
1. ***Continental Noise***
Let's give the noise used in the previous section a name, called continental noise.
Because it determines whether the current point is ocean or land (noise<-0.5 ? Ocean : Land), and if it is land, it has a base height (20/45/60/...).

2. ***Erosion Noise***
+ Making an analogy, *continental noise* is placing layered blocks on the map. The appearance of the blocks is smooth and rounded. If we want this map to have a more natural appearance, we need to appropriately chisel/cut these blocks so that their surface has more fluctuations.
+ For plateau terrain, such erosion will transform it from continuous smooth plateaus to rugged high-altitude mountain ranges. For plains terrain, such erosion will create valleys or rivers.
+ So, we define an erosion noise, which is also Simplex Noise. Erosion should appear more frequently, so we define its frequency as 0.3, but it should not have an earth-shattering impact on our terrain, so the corresponding linear interpolation output values will be much smaller.
**(The specific numerical values of linear interpolation can be defined by yourself. Different interpolations will bring dramatically different effects, which is also the most interesting part)**
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6c5174557af44c279c7ddcdaf40b76c7~tplv-k3u1fbpfcp-watermark.image?=512x384)

+ After adding erosion noise, we obtained the following terrain:
![1678198249602.jpg](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/08139f2509514bbf9afae9f3d2c90256~tplv-k3u1fbpfcp-watermark.image?=512x384)
    + Compared to using only continental noise, our new terrain has gained the ability to cut mountains and plateaus, so mountain ranges appeared on plateaus, and the transition between plateaus and mountains, mountains and plains has become more natural.
    
3. ***Peak-Valley Fluctuation Noise***
+ The terrain generation above has already achieved quite good results. We can define the terrain appearance we need based on interpolation or noise density, but a small part of the terrain still presents very flat planes or slopes; for example, in the image below, the slope on the left is quite smooth because it is generated by linear interpolation, and the small plane on the right is also relatively flat, only experiencing slight erosion with some fluctuations.
![1678198749890.jpg](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a2d28fc2756743809e9618f37485dc47~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ To make the whole more natural, we add another peak-valley fluctuation noise, which should have a relatively high frequency but the smallest terrain impact capability. Its biggest role is to make our terrain more natural and more "unintentional".
+ We define its frequency as 0.8 and also set up a linear interpolation
![1678199041656.jpg](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9c9b10f81d784b4e8633e370d3e4cd57~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ Applying peak-valley fluctuation noise to our terrain generation, we can obtain the following effect:
![1678199198629.jpg](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/efc3a80d05024f0aafa5c89129c37dfb~tplv-k3u1fbpfcp-watermark.image?=512x384)
The new terrain effect will not have significant differences from the previous one, but this helps us have more natural transitions in flat areas/slope areas.

### Terrain Generation Complete
Based on the combined use of the above three noises and their corresponding linear interpolations, we can already create a fairly rich and exciting terrain.
+ Summary of the three noises and their corresponding core functions:
1. **Continental Noise**: Used to define whether a unit cell is land or ocean. If it is land, give it an initial height.
2. **Erosion Noise**: Used to erode the original plains, mountains, and plateaus, carving away overly smooth planes/slopes to form more natural transitions between terrains and richer terrain effects.
3. **Peak-Valley Fluctuation Noise**: Has the smallest impact capability and highest fluctuation frequency, only used to give some overly smooth areas a more natural effect.

+ Adjusting the parameters and linear interpolation of the above three noises can achieve most of the effects we want, for example:
5. If we want larger plain areas and smaller plateau areas, we only need to adjust the linear interpolation of continental noise and increase the noise interval size occupied by plain height.
6. If we want different height plates to occupy larger areas, we only need to reduce the frequency of continental noise, reduce the frequency of fluctuations, which also gives each different plate a larger area.
7. If we want steep canyons and cliffs, we only need to add a steep downward depression in the linear interpolation of erosion noise, allowing plates in this interval to quickly dig out a piece, forming canyon and cliff effects.

## 2. Surface Climate Generation
### Creating Climate Properties Based on Temperature and Humidity
+ Same as the previous section, we use SimplexNoise to generate the world's temperature and humidity maps, then we can set the climate based on the following factors:
    1. Temperature
    2. Humidity
    3. Base elevation (can be obtained using continental noise values and their linear interpolation -- ocean, plains, plateaus, etc.)
+ Climate generation examples:
    + For example, extremely high temperature and low humidity scenarios can be set as deserts
    + Coastal, temperate and suitable humidity low-altitude areas are set as temperate forests
    + Low-temperature high mountain areas are set as snowy mountains/snowy forests
    + ...
+ We set core properties for each climate
    + Surface block type: grassland/snow/withered grassland
    + Surface vegetation density
    + Surface vegetation group array: for example, deserts have cacti, temperate forests have birch, oak, etc.

### Applying Climate to Basic Terrain
+ After obtaining the basic terrain in step 1, we obtained a world composed of stone. We perform the following steps in sequence:
    + According to climate, replace the N layers of stone on the surface with the corresponding surface blocks of the climate
    + Set empty blocks below sea level height to water
    + Add vegetation (trees, grassland)

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e8046e3945b1437193952a2554254c3d~tplv-k3u1fbpfcp-watermark.image?)

# Summary
+ So far, we have a map generation framework with rich terrain style variations and high customizability. Based on this, we can use noise or other algorithms to add underground caves, cliffs, village clusters, animal groups, etc. to improve game content.
+ However, since the map is calculated and loaded in real-time during the game, if we want to implement a unique map corresponding to a random number seed, using multi-threading to calculate different map chunks concurrently can bring defects of non-idempotent and inconsistent map generation.



# Project Code
[YunmaoLeo/MinecraftBasedOnDirectX](https://github.com/YunmaoLeo/MinecraftBasedOnDirectX)
## USE GUIDE:
0. Clone the repository and switch to the master branch
1. Open the ModelViewer.sln project using Rider or VisualStudio
2. World size display settings
    + In ModelViewer.cpp, set worldMap = new WorldMap(number of chunks on screen, chunk size, number of threads)
3. Map generation parameter settings
    + Enter WorldGenerator.cpp
    + Set their parameters in the various noise initialization areas at the top
    + Set the linear interpolation definition corresponding to each different noise
    + Set the surface block type, vegetation density, etc. corresponding to different climates
    + Set the climate corresponding to various attribute values, see getRealHeightAndBiomes()
```
//SimplexNoise corresponding to different noises
SimplexNoise continent(0.05, 1, 2, 0.5);
SimplexNoise erosion(0.1, 1, 2, 0.5);
SimplexNoise peaksValleys(0.5, 1, 2, 0.5);
SimplexNoise temperature(0.01, 1, 2, 0.5);
SimplexNoise humidity(0.01, 1, 2, 0.5);
SimplexNoise caves(0.8, 1, 2, 0.5);

//Initial settings for different climates
Biomes BarrenIceField(GrassSnow, Dirt, 7, 0.01, 20, 1);
Biomes InlandForest(GrassWilt, Dirt, 7, 0.01, 15, 1);
Biomes InlandPlain(GrassWilt, Dirt, 7, 0.05, 0,0);
Biomes Desert(Sand, Sand, 12, 0, 0, 0);
Biomes FlourishIceField(GrassSnow, Dirt, 9, 0.05, 15, 1);
Biomes Forest(Grass, Dirt, 8, 0.05, 6, 1);
Biomes RainForest(Grass, Dirt, 12, 0.10, 15, 2);
Biomes Savanna(Grass, Dirt, 12, 0.1, 0, 0);

//Interpolation definition corresponding to three terrain generation noises
std::map<float, float> ContinentalnessNodes = {
    {-1.0, ANOTHER_HEIGHT},
    {-0.5, ANOTHER_HEIGHT},
    ...
};

std::map<float, float> ErosionNodes = {
    {-1.0, 15},
    {-0.7, 10},
    ...
};

std::map<float, float> PeakValleysNodes = {
    {-1.0, -4},
    {-0.8, -3},
    ...
};
```

4. Switch to release mode, compile the project and run
5. By default, only blocks adjacent to air are rendered. You can delete blocks with left click and add a grass block with right click to perform some simple operations.
# References
[The World Generation of Minecraft - Alan Zucconi](https://www.alanzucconi.com/2022/06/05/minecraft-world-generation/)

[Reinventing Minecraft world generation by Henrik Kniberg - YouTube](https://www.youtube.com/watch?v=ob3VwY4JyzE&t=2554s)
