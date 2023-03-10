# 前言
## 1. 基于文中方法实现的效果预览：
+ 平原
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/944ba9f3b1c641248ff8ff8ec5f4029d~tplv-k3u1fbpfcp-watermark.image?=512x384)
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f04ce974460b4d838f4903e2253f1cc7~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ 内陆山地
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3f771a5bfaa84cdf84fb9ac64ff15112~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ 沙漠
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/20f7c2ceec924d25afbe271e6302bf7e~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ 热带丛林
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/321d06bc8e0a4fafa11d26a8db98d29e~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ 雪地&雪山
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/51a6ce32050a49acbcfc3297e9a611fe~tplv-k3u1fbpfcp-watermark.image?=512x384)
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0b10e5e1d1ee4652bb7b47045babbd5b~tplv-k3u1fbpfcp-watermark.image?=512x384)

## 2. 方法概览
我们基于三组Simplex Noise（基础海拔、地形侵蚀、波谷起伏）与对应的线性插值生成地图高度图，结合使用噪声生成的温湿度图来判定生物群落特征（植被密度、地表类型），生成多种不同组合的地形地貌。

# 方案实现
## 1. 地图生成步骤细分
对于沙盒游戏内的基础地图来说，我们从底至顶进行划分：
1. 地形高度：这一环节是所有步骤的基础，我们随机生成每一个单位点的高度，可以获取基础的地形样貌，如湖泊、平原、丘陵、山地、高原、山谷。
2. 气候：基于不同的温湿度水平，结合地形高度和与海洋的距离，获取世界地图的气候组成，
3. 地表地貌：我们可以通过气候和当前地形来划分不同位置的地貌，例如雪山的地表是雪地，湖泊的地表是水面，树林的地表是草地，内陆干旱高原的地表是枯草地。
4. 地底洞穴
5. 装饰层：植被（树木、草）、动物群、村庄等基于已有地形与气候添加的物体群，

接着，我们逐步来看每一层的实现细节
## 2. 地形高度随机生成
对于生成随机地形高度，我们可以很自然地利用随机数。  
### 白噪声地形
+ 简单地轮询调用``random.nextInt(MAX_HEIGHT)``，可以获取一个白噪音地形（也就是，点与点之间的高度并无关系，全部独立与彼此），我们随即获得了以下无规则地形。
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b335a77371df410ba7d16c15b5ae140a~tplv-k3u1fbpfcp-watermark.image?=512x384)

### 初试：基于单纯的Simplex Noise生成地形
+ 显然，我们需要渐进式的随机来为我们提供有连续性的地图高度，游戏随机生成应用广泛的随机噪声算法很多，我们只需要选取适合的即可。
    + **Perlin Noise**: 在随机数的基础上应用渐变和插值，使得点位之间产生自然、连续的随机效果。
    + **Simplex Noise**: Perlin Noise的改进版，通过高维空间中的等边多面体来生成噪音，再通过投影和插值计算出结果，有更快的运行效率和更好的表现，这也是我们选用的噪声。
    + **Value Noise**: 基于网格噪声，在网格内进行采样并插值生成噪音，相较于Perlin Noise，层次更弱效果稍差，但运行速度较快。
    + **Fractal Noise**: 生成一张基础噪声图像，对该图像进行多次缩放、平移和旋转操作来叠加生成复杂、自然的噪声图像。

+ 使用Simplex Noise的核心参数
    + **Frequency** 频率: 频率越高，生成噪声的起伏越频繁。
    + **Lacunarity** 稀疏度：控制噪声中的大体形状，较高的稀疏度会生成不规则的形状，较低的稀疏度会生成光滑的形状。
    + **Amplitude** 振幅：控制噪声的放大倍数（纵向拉伸或挤压）

+ 地形生成  
对于每一个单位点的高度生成，我们使用如下方法：
    SimplexNoise接受一个单位坐标，输出一个(-1,1)的值，我们认为-1代表最低，1代表最高，中间值按照比例缩放。
```C++
//x, y分别为点位的坐标
float noise = SimplexNoise(x,y)
//指定一个世界的最低地表高度
int SURFACE_HEIGHT = 20;
int height = SURFACE_HEIGHT + (noise+1)/2 * (128); 
```
我们的Simplex Noise具有如下参数设定  
+ Frequency: 0.3
+ Amplitude: 1
+ Lacunarity: 2

使用这一SimplexNoise，我们生成了如下地形，规律性的起伏带来了山脉连绵的感觉，其间我们设定的最低SURFACE_HEIGHT也带来了一些基础的河流地，相较白噪音地形，拥有了更好的效果，但对于一个地形来说，他有些过于规则了，且缺少变化。
![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/33663525b0cf460ca0915f89d1e5770a~tplv-k3u1fbpfcp-watermark.image?=512x384)
当我们降低SimplexNoise的参数Frequency至0.1，就获得了山体更大，高度更小，起伏更平缓的地形。
![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/afa539f7d9dd43e29c911586b37d1409~tplv-k3u1fbpfcp-watermark.image?=512x384)

### 设定线性插值来实现变化的基础高度
+ 使用一层简单的SimplexNoise，让我们获得了一个渐近变化的地形；  
+ 然而这样的地形似乎过于单调了，一直是基于SURFACE_HEIGHT的连绵起伏。
+ 对于真实的地形，我们希望他具有多种不同的基础高度，比如：
    + 基于SURFACE_HEIGHT的起伏形成丘陵
    + 基于SURFACE_HEIGHT+20的起伏形成山地
    + 基于SURFACE_HEIGHT+50的起伏形成高原

如下的折线图展现了我们如何对输出的噪声进行线性插值，我们输入一个(-1,1)范围内的噪声，根据定义的点位进行插值，输出高度。
+ 这是我们在上一章节尝试的线性插值，noise=-1标志着最低点，noise=1标志着最高点
![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f086e33031384a84a532ff95d40c0008~tplv-k3u1fbpfcp-watermark.image?)
+ 为了让我们的地形拥有更多层次，我们尝试按照海平面、平地、山地、高原这样的层次设立插值，如下图：
    + noise低于-0.6的位置，被我们全部设定为海平面高度
    + noise处于-0.6~-0.5之间，我们认定为是海面到平原的过渡段
    + noise处于-0.5~0之间，设定为平原，这里拥有较低的海拔
    + noise处于0~0.4之间，设定为高度逐渐攀升的山地
    + noise处于0.5及以上，设定为高原
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7c97101f667947feb4552dbdbc0907ea~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ 基于这一插值，我们获得了如下地形效果，见下图

![1678198009840.jpg](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ee2fb019f13a444bbe7787002452e78b~tplv-k3u1fbpfcp-watermark.image?=512x384)
    + 最靠近我们的，海拔最低的，是海洋/湖泊
    + 较高一层的，是我们设定的平原，广阔且平坦
    + 更高一层且产生波动的，是我们寻求的山地
    + 最远，且最高的，是高原
+ 基于这一线性插值产生的**地形效果拥有了更多的层次**，但也同样充斥着**过于平滑的奇怪观感**，例如湖泊到平原的过度永远是一个斜坡，高原或平原上没有任何的起伏。
### 把三组不同含义的噪声组合使用
在上一章节中，我们尝试使用一组噪声及其线性插值，实现了海洋、平原、山地和高地的粗略效果，我们不妨思考一下，是否能够组合多组噪声，让他们融合成为一个更自然的地形。
1. ***大陆板块噪声***  
我们给上一章节使用的噪声定义一个名字，称为大陆板块噪声。  
因为它决定了当前点位究竟是海洋还是陆地(noise<-0.5 ? Ocean : Land)，如果是陆地，那他拥有一个基础高度(20/45/60/...)。

2. ***侵蚀噪声***  
+ 做一个类比，*大陆板块噪声*正在往地图上放置一个一个有层次的积木，积木的外观是平滑圆润的，如果我们希望这个地图拥有更自然的外观，就需要适当凿开/切割这个积木，让其表面拥有更多波动；  
+ 对于高原地形来说，这样的侵蚀会让它从连绵的平滑高原，变为崎岖的高海拔山脉，对于平原地形来说，这样的侵蚀会让其产生谷地或河流。  
+ 所以，我们定义一个侵蚀噪音，它同样是Simplex Noise，侵蚀的出现应该是更频繁的，所以我们将其frequency定义为0.3，但它不应对我们的地形产生天翻地覆的影响，所以其对应的线性插值输出的数值会小得多。   
**(线性插值的具体数值自己定义即可，不同的插值会带来天翻地覆的效果变化，这也是最有趣的部分)**
![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6c5174557af44c279c7ddcdaf40b76c7~tplv-k3u1fbpfcp-watermark.image?=512x384)

+ 添加侵蚀噪声后，我们获得了如下的地形：
![1678198249602.jpg](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/08139f2509514bbf9afae9f3d2c90256~tplv-k3u1fbpfcp-watermark.image?=512x384)
    + 对比单纯使用大陆板块噪声，我们的新地形拥有了切割山地和高原的能力，因而高原上出现了山脉，高原和山地，山地和平原之间的过渡也更自然了。
    
3.***波谷起伏噪声***  
+ 上面的地形生成，已经有了相当不错的效果，我们可以根据插值，或是噪声的密度来定义我们需要的地形样貌了，但地形中的一小部分，仍然呈现出非常平整的平面或斜面；比如下图中，左侧的斜坡因为是线性插值生成，相当平滑，右侧的小平面也较为平整，只是经历了小许的侵蚀出现了波动。
![1678198749890.jpg](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a2d28fc2756743809e9618f37485dc47~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ 为了让整体更自然，我们再添加一个波谷起伏噪声，它应该拥有比较高的频率，但拥有最小的地形影响能力，它最大的作用是让我们的地形变得更自然，更“不刻意”。
+ 我们将其频率定义为0.8，同样设立一个线性插值
![1678199041656.jpg](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9c9b10f81d784b4e8633e370d3e4cd57~tplv-k3u1fbpfcp-watermark.image?=512x384)
+ 对我们的地形生成应用波谷起伏噪声，就可获得如下效果：
![1678199198629.jpg](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/efc3a80d05024f0aafa5c89129c37dfb~tplv-k3u1fbpfcp-watermark.image?=512x384)
新的地形效果与此前的并不会有显著差异，但这帮助我们在平面区域/斜面区域都拥有了更自然的过渡。

### 地形生成完成
基于以上三个噪声及其对应线性插值的组合使用，我们已经能够创造出一个相当丰富精彩的地貌了；  
+ 总结三个噪声及其对应的核心作用：
1. **大陆板块噪声**：用于定义一个单位格是陆地还是海洋，如果是陆地，那么赋予其一个初始高度
2. **侵蚀噪声**：用于对原有的平原、山地、高原进行侵蚀，雕琢掉过于平滑的平面/斜面，形成更自然的地形间过渡和更丰富的地形效果。 
3. **波谷起伏噪声**：影响能力最小，起伏频率最高，仅用于让一些过于平滑的区域拥有更自然的效果。

+ 对上述三个噪声的参数、线性插值进行调整，可以实现我们想要的大部分效果，例如：
5. 如果我们希望平原的面积更大，高原的面积更小，只需要在大陆板块噪声的线性插值进行调整，增加平原高度所占的noise区间大小即可。
6. 如果我们希望不同的高度板块占的面积更大，只需要调小大陆板块噪声的frequency，减小起伏的频率，也就让每一种不同的版块有了更大的面积。
7. 如果我们希望拥有险峻的峡谷和悬崖，只需要在侵蚀噪声的线性插值中，添加一段陡峭的下凹陷即可，让处在这区间的板块快速挖去一块，形成峡谷和悬崖效果。

## 2. 地表气候生成
### 基于温湿度创建气候属性
+ 一章节同理，我们利用SimplexNoise生成世界的温度、湿度图，接着我们可以根据以下几个因素设定气候:
    1. 温度
    2. 湿度
    3. 基础海拔（利用大陆板块噪声值及其线性插值可得--海洋、平原、高原等
+ 气候生成示例：
    + 例如极高温，低湿度的场景我们可以设定为沙漠
    + 近海、温度湿度适宜的低海拔地区设置为温带树林
    + 温度低的高山区域设置为雪山/雪树林
    + ...
+ 我们给每种气候设置核心属性
    + 地表方块类型：草地/雪地/枯草地
    + 地表植被密度
    + 地表植被群数组：例如沙漠有仙人掌，温带树林有白桦木、橡树等

### 将气候应用在基础地形上
+ 在步骤1中获取了基础地形后，我们获得了由石头组成的世界，我们依次执行以下步骤：
    + 根据气候，替换地表的N层石头为气候对应的地表方块
    + 将低于海平面高度的空方块设置为水
    + 添加植被（树木、草地）

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e8046e3945b1437193952a2554254c3d~tplv-k3u1fbpfcp-watermark.image?)

# 总结
+ 至此，我们拥有了一个地貌样式变化丰富，可自定义程度高的地图生成框架，基于此，我们可以添加利用噪声或是其他算法添加地底洞穴、悬崖峭壁、村庄群落、动物群落等完善游戏内容。
+ 然而，由于地图是在游戏进行过程中及时计算、加载的，如果我们希望实现一个随机数种子对应一个唯一的地图，使用多线程并发计算不同地图区块就可以会带来地图生成不幂等不一致的缺陷。



# 项目代码
[YunmaoLeo/MinecraftBasedOnDirectX](https://github.com/YunmaoLeo/MinecraftBasedOnDirectX)
## USE GUIDE：
0. Clone代码库，切换到master分支
1. 使用Rider或VisualStudio打开ModelViewer.sln项目
2. 同屏展示的世界大小设置
    + ModelViewer.cpp中，设定worldMap = new WorldMap(同屏区块数, 区块尺寸, 线程数)
3. 地图生成的参数设定
    + 进入WorldGenerator.cpp
    + 在最上方的各个不同的噪声初始化的区域设置他们的参数
    + 设置每个不同的噪声对应的线性插值定义
    + 设置不同气候对应的地表方块类型，植被密度等
    + 设置各项属性值对应的气候，见getRealHeightAndBiomes()
```
//不同噪音对应的SimplexNoise
SimplexNoise continent(0.05, 1, 2, 0.5);
SimplexNoise erosion(0.1, 1, 2, 0.5);
SimplexNoise peaksValleys(0.5, 1, 2, 0.5);
SimplexNoise temperature(0.01, 1, 2, 0.5);
SimplexNoise humidity(0.01, 1, 2, 0.5);
SimplexNoise caves(0.8, 1, 2, 0.5);

//不同气候的初始设置
Biomes BarrenIceField(GrassSnow, Dirt, 7, 0.01, 20, 1);
Biomes InlandForest(GrassWilt, Dirt, 7, 0.01, 15, 1);
Biomes InlandPlain(GrassWilt, Dirt, 7, 0.05, 0,0);
Biomes Desert(Sand, Sand, 12, 0, 0, 0);
Biomes FlourishIceField(GrassSnow, Dirt, 9, 0.05, 15, 1);
Biomes Forest(Grass, Dirt, 8, 0.05, 6, 1);
Biomes RainForest(Grass, Dirt, 12, 0.10, 15, 2);
Biomes Savanna(Grass, Dirt, 12, 0.1, 0, 0);

//三种地形生成噪音对应的插值定义
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

4. 切换至release模式，编译项目并运行
5. 默认情况下只渲染与空气毗邻的方块，可以通过左键删除方块，右键添加一个草方块来执行一些简单的操作。
# 资料参考
[The World Generation of Minecraft - Alan Zucconi](https://www.alanzucconi.com/2022/06/05/minecraft-world-generation/)

[Reinventing Minecraft world generation by Henrik Kniberg - YouTube](https://www.youtube.com/watch?v=ob3VwY4JyzE&t=2554s)
