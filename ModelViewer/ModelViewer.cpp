//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#include <complex>
#include <iostream>

#include "GameCore.h"
#include "CameraController.h"
#include "BufferManager.h"
#include "Camera.h"
#include "CommandContext.h"
#include "TemporalEffects.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "PostEffects.h"
#include "SSAO.h"
#include "FXAA.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "ParticleEffectManager.h"
#include "GameInput.h"
#include "glTF.h"
#include "Renderer.h"
#include "Model.h"
#include "ModelLoader.h"
#include "ShadowCamera.h"
#include "Display.h"
#include "LightManager.h"
#include "SimplexNoise.h"
#include "ThreadPool.h"
#include "Blocks/BlockResourceManager.h"
#include "World/WorldMap.h"
#include "World/World.h"
#include "World/Chunk.h"

#define LEGACY_RENDERER

using namespace GameCore;
using namespace Math;
using namespace Graphics;
using namespace std;

using Renderer::MeshSorter;

class ModelViewer : public GameCore::IGameApp
{
public:
    ModelViewer(void)
    {
    }

    void PickItem(int sx, int sy);
    virtual void Startup(void) override;
    virtual void Cleanup(void) override;

    virtual void Update(float deltaT) override;
    void RenderBlocks(MeshSorter& sorter, MeshSorter::DrawPass pass, GraphicsContext& context,
                      GlobalConstants& globals);
    void RenderShadowBlocks(MeshSorter& sorter, MeshSorter::DrawPass pass, GraphicsContext& context,
                            GlobalConstants& globals, Matrix4 matrix);
    virtual void RenderScene(void) override;

private:
    Camera m_Camera;
    unique_ptr<CameraController> m_CameraController;

    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    D3D12_VIEWPORT subViewPort;
    D3D12_RECT subScissor;

    ModelInstance m_ModelInst;
    Chunk world_block;
    WorldMap* worldMap;
    ShadowCamera m_SunShadow;
};

NumVar ShadowDimX("Sponza/Lighting/Shadow Dim X", 10000, 1000, 10000, 100);
NumVar ShadowDimY("Sponza/Lighting/Shadow Dim Y", 10000, 1000, 10000, 100);
NumVar ShadowDimZ("Sponza/Lighting/Shadow Dim Z", 10000, 1000, 10000, 100);
CREATE_APPLICATION(ModelViewer)

ExpVar g_SunLightIntensity("Viewer/Lighting/Sun Light Intensity", 4.0f, 0.0f, 16.0f, 0.1f);
NumVar g_SunOrientation("Viewer/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
NumVar g_SunInclination("Viewer/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);
NumVar ModelUnitSize("Model/Unit Size", 500.0f, 100.0f, 1000.0f, 100.0f);

void ChangeIBLSet(EngineVar::ActionType);
void ChangeIBLBias(EngineVar::ActionType);

DynamicEnumVar g_IBLSet("Viewer/Lighting/Environment", ChangeIBLSet);
std::vector<std::pair<TextureRef, TextureRef>> g_IBLTextures;
NumVar g_IBLBias("Viewer/Lighting/Gloss Reduction", 2.0f, 0.0f, 10.0f, 1.0f, ChangeIBLBias);

void ChangeIBLSet(EngineVar::ActionType)
{
    int setIdx = g_IBLSet - 1;
    if (setIdx < 0)
    {
        Renderer::SetIBLTextures(nullptr, nullptr);
    }
    else
    {
        auto texturePair = g_IBLTextures[setIdx];
        Renderer::SetIBLTextures(texturePair.first, texturePair.second);
    }
}

void ChangeIBLBias(EngineVar::ActionType)
{
    Renderer::SetIBLBias(g_IBLBias);
}

#include <direct.h> // for _getcwd() to check data root path

void LoadIBLTextures()
{
    char CWD[256];
    _getcwd(CWD, 256);

    Utility::Printf("Loading IBL environment maps\n");

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(L"Textures/*_diffuseIBL.dds", &ffd);

    g_IBLSet.AddEnum(L"None");

    if (hFind != INVALID_HANDLE_VALUE)
        do
        {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            std::wstring diffuseFile = ffd.cFileName;
            std::wstring baseFile = diffuseFile;
            baseFile.resize(baseFile.rfind(L"_diffuseIBL.dds"));
            std::wstring specularFile = baseFile + L"_specularIBL.dds";

            TextureRef diffuseTex = TextureManager::LoadDDSFromFile(L"Textures/" + diffuseFile);
            std::wcout <<"find skybox texture: "<< baseFile << std::endl;
            if (diffuseTex.IsValid())
            {
                TextureRef specularTex = TextureManager::LoadDDSFromFile(L"Textures/" + specularFile);
                if (specularTex.IsValid())
                {
                    g_IBLSet.AddEnum(baseFile);
                    g_IBLTextures.push_back(std::make_pair(diffuseTex, specularTex));
                }
            }
        }
        while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

    Utility::Printf("Found %u IBL environment map sets\n", g_IBLTextures.size());

    if (g_IBLTextures.size() > 0)
        g_IBLSet.Increment();
}

int Chunk::blockId = 0;

void ModelViewer::PickItem(int sx, int sy)
{
    Matrix4 P = m_Camera.GetProjMatrix();
    float vx = (+2.0f*sx / g_SceneColorBuffer.GetWidth() - 1.0f) / P.GetX().GetX();
    float vy = (-2.0f*sy / g_SceneColorBuffer.GetHeight() + 1.0f) / P.GetY().GetY();

    XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

    XMMATRIX V = m_Camera.GetViewMatrix();
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V),V);
}

void ModelViewer::Startup(void)
{
    // ?????????????????????
    std::cout <<"Sizeof Block: " << sizeof(Block) << std::endl;
    SimplexNoise::setSeed(20);
    MotionBlur::Enable = true;

    TemporalEffects::EnableTAA = true;
    FXAA::Enable = false;

    // ?????????
    PostEffects::EnableHDR = true;
    PostEffects::EnableAdaptation = true;

    // Screen Space Ambient Occlusion ???????????????????????????
    SSAO::Enable = true;

    Renderer::Initialize();
    BlockResourceManager::initBlocks();
    LoadIBLTextures();

    std::wstring gltfFileName;

    bool forceRebuild = true;
    uint32_t rebuildValue;
    if (CommandLineArgs::GetInteger(L"rebuild", rebuildValue))
        forceRebuild = rebuildValue != 0;

    if (CommandLineArgs::GetString(L"model", gltfFileName) == true)
    {
        m_ModelInst = Renderer::LoadModel(L"Sponza/sponza.h3d", forceRebuild);
        m_ModelInst.Resize(100.0f * m_ModelInst.GetRadius());
        OrientedBox obb = m_ModelInst.GetBoundingBox();
        float modelRadius = Length(obb.GetDimensions()) * 0.5f;
        const Vector3 eye = obb.GetCenter() + Vector3(modelRadius * 0.5f, 0.0f, 0.0f);
        m_Camera.SetEyeAtUp(eye, Vector3(kZero), Vector3(kYUnitVector));
    }
    else
    {
        worldMap = new WorldMap(27,16,2);
        
        // world_block = WorldBlock(Vector3(0, 0, 0), 16);
        MotionBlur::Enable = false;
        //Lighting::CreateRandomLights(m_ModelInst.m_Model->m_BoundingBox.GetMin(),m_ModelInst.m_Model->m_BoundingBox.GetMax());
    }

    m_Camera.SetZRange(1.0f, 15000.0f);
    m_Camera.SetPosition({0,128.0f*World::UnitBlockSize, 0});
    if (gltfFileName.size() == 0)
    {
        m_CameraController.reset(new FlyingFPSCamera(m_Camera, Vector3(kYUnitVector)));
        //m_CameraController.reset(new OrbitCamera(m_Camera, m_ModelInst.GetBoundingSphere(), Vector3(kYUnitVector)));
    }
    else
        m_CameraController.reset(new OrbitCamera(m_Camera, m_ModelInst.GetBoundingSphere(), Vector3(kYUnitVector)));
}

void ModelViewer::Cleanup(void)
{
    m_ModelInst = nullptr;
    // world_block.CleanUp();

    g_IBLTextures.clear();

    Lighting::Shutdown();
    Renderer::Shutdown();
}

namespace Graphics
{
    extern EnumVar DebugZoom;
}

void ModelViewer::Update(float deltaT)
{
    ScopedTimer _prof(L"Update State");
    
    if (GameInput::IsFirstPressed(GameInput::kLShoulder))
        DebugZoom.Decrement();
    else if (GameInput::IsFirstPressed(GameInput::kRShoulder))
        DebugZoom.Increment();

    m_CameraController->Update(deltaT);

    Vector3 ori = m_Camera.GetPosition();
    Vector3 dir = Normalize(m_Camera.GetForwardVec());
    if (GameInput::IsFirstReleased(GameInput::kMouse0))
    {
        worldMap->DeleteBlock(ori,dir);
    }
    if (GameInput::IsFirstReleased(GameInput::kMouse1))
    {
        worldMap->PutBlock(ori,dir, Grass);
    }
    
    
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Update");

    gfxContext.Finish();

    // We use viewport offsets to jitter sample positions from frame to frame (for TAA.)
    // D3D has a design quirk with fractional offsets such that the implicit scissor
    // region of a viewport is floor(TopLeftXY) and floor(TopLeftXY + WidthHeight), so
    // having a negative fractional top left, e.g. (-0.25, -0.25) would also shift the
    // BottomRight corner up by a whole integer.  One solution is to pad your viewport
    // dimensions with an extra pixel.  My solution is to only use positive fractional offsets,
    // but that means that the average sample position is +0.5, which I use when I disable
    // temporal AA.
    TemporalEffects::GetJitterOffset(m_MainViewport.TopLeftX, m_MainViewport.TopLeftY);

    m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
    m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    m_MainScissor.left = 0;
    m_MainScissor.top = 0;
    m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
    m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();

    float depth_vp_ratio = 0.35;

    subViewPort.Width = (float)g_SceneColorBuffer.GetWidth() * depth_vp_ratio;
    subViewPort.Height = (float)g_SceneColorBuffer.GetHeight() * depth_vp_ratio;
    subViewPort.MinDepth = 0.0f;
    subViewPort.MaxDepth = 1.0f;
    subViewPort.TopLeftX = (float)g_SceneColorBuffer.GetWidth() * (1 - depth_vp_ratio);
    subViewPort.TopLeftY = (float)g_SceneColorBuffer.GetHeight() * (1 - depth_vp_ratio);

    subScissor.left = (float)g_SceneColorBuffer.GetWidth() * (1 - depth_vp_ratio);
    subScissor.top = (float)g_SceneColorBuffer.GetHeight() * (1 - depth_vp_ratio);
    subScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
    subScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();
}

void ModelViewer::RenderBlocks(MeshSorter& sorter, MeshSorter::DrawPass pass, GraphicsContext& context,
                               GlobalConstants& globals)
{
    for (int i = 0; i < BlockResourceManager::BlockType::Air; i++)
    {
        auto type = static_cast<BlockResourceManager::BlockType>(i);
        sorter.ClearMeshes();
        sorter.currentBlockType = type;
        BlockResourceManager::getBlockRef(type).Render(sorter);
        sorter.Sort();
        sorter.RenderMeshes(pass, context, globals);
    }
}

void ModelViewer::RenderShadowBlocks(MeshSorter& sorter, MeshSorter::DrawPass pass, GraphicsContext& context,
                                     GlobalConstants& globals, Matrix4 matrix)
{
    for (int i = 0; i < BlockResourceManager::BlockType::Air; i++)
    {
        auto type = static_cast<BlockResourceManager::BlockType>(i);
        sorter.ClearMeshes();
        sorter.currentBlockType = type;
        BlockResourceManager::getBlockRef(type).Render(sorter);
        sorter.Sort();
        sorter.RenderMeshes(pass, context, globals, matrix);
    }
}

void ModelViewer::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");
    uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2();
    const D3D12_VIEWPORT& viewport = m_MainViewport;
    const D3D12_RECT& scissor = m_MainScissor;

    ParticleEffectManager::Update(gfxContext.GetComputeContext(), Graphics::GetFrameTime());

    // Update global constants
    float costheta = cosf(g_SunOrientation);
    float sintheta = sinf(g_SunOrientation);
    float cosphi = cosf(g_SunInclination * 3.14159f * 0.5f);
    float sinphi = sinf(g_SunInclination * 3.14159f * 0.5f);

    Vector3 SunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));
    Vector3 ShadowBounds = Vector3(m_ModelInst.GetRadius());
    //m_SunShadow.UpdateMatrix(-SunDirection, m_ModelInst.GetCenter(), ShadowBounds,
    m_SunShadow.UpdateMatrix(-SunDirection, Vector3(0, -500.0f, 0), Vector3(ShadowDimX, ShadowDimY, ShadowDimZ),
                             (uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16);

    GlobalConstants globals;
    globals.ViewProjMatrix = m_Camera.GetViewProjMatrix();
    globals.SunShadowMatrix = m_SunShadow.GetShadowMatrix();
    globals.CameraPos = m_Camera.GetPosition();
    globals.SunDirection = SunDirection;
    globals.SunIntensity = Vector3(Scalar(g_SunLightIntensity));
    // world_block.CopyDepthBuffer(gfxContext);
    // Begin rendering depth
    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    MeshSorter sorter(MeshSorter::kDefault);
    sorter.SetCamera(m_Camera);
    sorter.SetViewport(viewport);
    sorter.SetScissor(scissor);
    sorter.SetDepthStencilTarget(g_SceneDepthBuffer);
    sorter.AddRenderTarget(g_SceneColorBuffer);

    {
        ScopedTimer _prof(L"renderVisibleBlocksInCPU", gfxContext);
        worldMap->renderVisibleBlocks(m_Camera, gfxContext);
    }

    {
        ScopedTimer _prof(L"Depth Pre-Pass", gfxContext);
        RenderBlocks(sorter, MeshSorter::kZPass, gfxContext, globals);
    }

    SSAO::Render(gfxContext, m_Camera);
    if (!SSAO::DebugDraw)
    {
        ScopedTimer _outerprof(L"Main Render", gfxContext);

        {   
            ScopedTimer _prof(L"Sun Shadow Map", gfxContext);
        
            MeshSorter shadowSorter(MeshSorter::kShadows);
            shadowSorter.SetCamera(m_Camera);
            shadowSorter.SetDepthStencilTarget(g_ShadowBuffer);
            RenderShadowBlocks(shadowSorter, MeshSorter::kZPass, gfxContext, globals, m_SunShadow.GetViewProjMatrix());
        }

        gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        gfxContext.ClearColor(g_SceneColorBuffer);

        {
            ScopedTimer _prof(L"Render Color", gfxContext);
            gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
            gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
            gfxContext.SetViewportAndScissor(viewport, scissor);

            RenderBlocks(sorter, MeshSorter::kOpaque, gfxContext, globals);
        }

        Renderer::DrawSkybox(gfxContext, m_Camera, viewport, scissor);
        
        RenderBlocks(sorter, MeshSorter::kTransparent, gfxContext, globals);
    }
    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
    // Some systems generate a per-pixel velocity buffer to better track dynamic and skinned meshes.  Everything
    // is static in our scene, so we generate velocity from camera motion and the depth buffer.  A velocity buffer
    // is necessary for all temporal effects (and motion blur).
    MotionBlur::GenerateCameraVelocityBuffer(gfxContext, m_Camera, true);

    TemporalEffects::ResolveImage(gfxContext);

    //ParticleEffectManager::Render(gfxContext, m_Camera, g_SceneColorBuffer, g_SceneDepthBuffer,  g_LinearDepth[FrameIndex]);
    
    if (DepthOfField::Enable)
        DepthOfField::Render(gfxContext, m_Camera.GetNearClip(), m_Camera.GetFarClip());
    else
        MotionBlur::RenderObjectBlur(gfxContext, g_VelocityBuffer);
    gfxContext.Finish();
}
