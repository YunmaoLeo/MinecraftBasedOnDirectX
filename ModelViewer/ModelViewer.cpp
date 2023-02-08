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
#include "Blocks/BlockResourceManager.h"
#include "World/World.h"
#include "World/WorldBlock.h"

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

    virtual void Startup(void) override;
    virtual void Cleanup(void) override;

    virtual void Update(float deltaT) override;
    virtual void RenderScene(void) override;

private:
    Camera m_Camera;
    unique_ptr<CameraController> m_CameraController;

    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    D3D12_VIEWPORT subViewPort;
    D3D12_RECT subScissor;

    ModelInstance m_ModelInst;
    WorldBlock world_block;
    ShadowCamera m_SunShadow;
};


NumVar ShadowDimX("Sponza/Lighting/Shadow Dim X", 5000, 1000, 10000, 100);
NumVar ShadowDimY("Sponza/Lighting/Shadow Dim Y", 3000, 1000, 10000, 100);
NumVar ShadowDimZ("Sponza/Lighting/Shadow Dim Z", 3000, 1000, 10000, 100);
CREATE_APPLICATION(ModelViewer)

ExpVar g_SunLightIntensity("Viewer/Lighting/Sun Light Intensity", 4.0f, 0.0f, 16.0f, 0.1f);
NumVar g_SunOrientation("Viewer/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
NumVar g_SunInclination("Viewer/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);
NumVar ModelUnitSize("Model/Unit Size", 500.0f, 100.0f, 1000.0f, 100.0f);
BoolVar openOcclusionCulling("Model/OcclusionOpen", true);

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


ID3D12Resource* m_queryResult;
ID3D12QueryHeap* m_queryHeap;
D3D12_QUERY_HEAP_DESC occlusionQueryHeapDesc = {};

void ModelViewer::Startup(void)
{
    // 初始化动态模糊
    MotionBlur::Enable = true;

    TemporalEffects::EnableTAA = true;
    FXAA::Enable = false;

    // 后处理
    PostEffects::EnableHDR = true;
    PostEffects::EnableAdaptation = true;

    // Screen Space Ambient Occlusion 屏幕空间环境光遮蔽
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
        // Load Model
        world_block = WorldBlock(Vector3(0, 0, 0), 2);
        std::cout << "blockSize" << world_block.blocks.size() << std::endl;
        m_ModelInst = BlockResourceManager::getBlock(BlockResourceManager::Torch).m_Model;
        m_ModelInst.Resize(300.0f);
        m_ModelInst.Translate({0,0,0});
        MotionBlur::Enable = false;
        //Lighting::CreateRandomLights(m_ModelInst.m_Model->m_BoundingBox.GetMin(),m_ModelInst.m_Model->m_BoundingBox.GetMax());
    }

    m_Camera.SetZRange(1.0f, 10000.0f);
    if (gltfFileName.size() == 0)
    {
        m_CameraController.reset(new FlyingFPSCamera(m_Camera, Vector3(kYUnitVector)));
        //m_CameraController.reset(new OrbitCamera(m_Camera, m_ModelInst.GetBoundingSphere(), Vector3(kYUnitVector)));
    }
    else
        m_CameraController.reset(new OrbitCamera(m_Camera, m_ModelInst.GetBoundingSphere(), Vector3(kYUnitVector)));

    occlusionQueryHeapDesc.Count = 3;
    occlusionQueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
    g_Device->CreateQueryHeap(&occlusionQueryHeapDesc, IID_PPV_ARGS(&m_queryHeap));

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    auto queryBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(8 * 3);
    g_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &queryBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_queryResult)
        );
}

void ModelViewer::Cleanup(void)
{
    m_ModelInst = nullptr;
    world_block.CleanUp();

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

    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Update");

    m_ModelInst.Update(gfxContext, deltaT);
    world_block.Update(gfxContext, deltaT);

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



void ModelViewer::RenderScene(void)
{
    std::cout << "start a render" << std::endl;
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

    // Begin rendering depth
    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    MeshSorter sorter(MeshSorter::kDefault);
    sorter.SetCamera(m_Camera);
    sorter.SetViewport(viewport);
    sorter.SetScissor(scissor);
    sorter.SetDepthStencilTarget(g_SceneDepthBuffer);
    sorter.AddRenderTarget(g_SceneColorBuffer);

    MeshSorter sorter2(MeshSorter::kDefault);
    sorter2.SetCamera(m_Camera);
    sorter2.SetViewport(viewport);
    sorter2.SetScissor(scissor);
    sorter2.SetDepthStencilTarget(g_SceneDepthBuffer);
    sorter2.AddRenderTarget(g_SceneColorBuffer);

    // m_ModelInst.Render(sorter);
    {
        ScopedTimer _BlocksRenderProf(L"BlocksRenderToSorter", gfxContext);
        // world_block.Render(sorter, m_Camera);
        world_block.blocks[0][0][0].Render(sorter);
        world_block.blocks[0][0][1].Render(sorter2);
        world_block.blocks[0][1][0].Render(sorter2);
        world_block.blocks[1][0][0].Render(sorter2);
        m_ModelInst.Render(sorter2);
    }

    {
        ScopedTimer _sortProf(L"Sorter sorts", gfxContext);
        sorter.Sort();
        sorter2.Sort();
    }
    {
        ScopedTimer _prof(L"Depth Pre-Pass", gfxContext);
        if (openOcclusionCulling)
        {
            gfxContext.SetPredication(m_queryResult, 0,D3D12_PREDICATION_OP_EQUAL_ZERO);
        }sorter.RenderMeshes(MeshSorter::kZPass, gfxContext, globals);
        gfxContext.SetPredication(nullptr, 0,D3D12_PREDICATION_OP_EQUAL_ZERO);
        // gfxContext.SetPredication(m_queryResult, 8,D3D12_PREDICATION_OP_EQUAL_ZERO);
        sorter2.RenderMeshes(MeshSorter::kZPass, gfxContext, globals);
    }

    SSAO::Render(gfxContext, m_Camera);
    if (!SSAO::DebugDraw)
    {
        ScopedTimer _outerprof(L"Main Render", gfxContext);

        // {
        //     ScopedTimer _prof(L"Sun Shadow Map", gfxContext);
        //
        //     MeshSorter shadowSorter(MeshSorter::kShadows);
        //     shadowSorter.SetCamera(m_Camera);
        //     shadowSorter.SetDepthStencilTarget(g_ShadowBuffer);
        //
        //     // m_ModelInst.Render(shadowSorter);
        //     world_block.Render(shadowSorter, m_Camera);
        //
        //     shadowSorter.Sort();
        //
        //     shadowSorter.RenderMeshes(MeshSorter::kZPass, gfxContext, globals, m_SunShadow.GetViewProjMatrix());
        // }

        {
        }

        gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        gfxContext.ClearColor(g_SceneColorBuffer);

        {
            ScopedTimer _prof(L"Render Color", gfxContext);
            gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
            gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
            gfxContext.SetViewportAndScissor(viewport, scissor);

            if (openOcclusionCulling)
            {
                gfxContext.SetPredication(m_queryResult, 0,D3D12_PREDICATION_OP_EQUAL_ZERO);
            }
            sorter.RenderMeshes(MeshSorter::kOpaque, gfxContext, globals);
            gfxContext.SetPredication(nullptr, 0,D3D12_PREDICATION_OP_EQUAL_ZERO);
            sorter2.RenderMeshes(MeshSorter::kOpaque, gfxContext, globals);
        }

        Renderer::DrawSkybox(gfxContext, m_Camera, viewport, scissor);
        //
        // sorter.RenderMeshes(MeshSorter::kTransparent, gfxContext, globals);
    }
    {
        MeshSorter sorter3(MeshSorter::kDefault);
        sorter3.SetCamera(m_Camera);
        sorter3.SetViewport(viewport);
        sorter3.SetScissor(scissor);
        sorter3.SetDepthStencilTarget(g_SceneDepthBuffer);
        
        Block& block = world_block.blocks[0][0][0];
        block.Render(sorter3);
        // block.model.m_Model
        // world_block.blocks[0][0][0].model.Resize(World::UnitBlockRadius);
        sorter3.Sort();
        

        gfxContext.BeginQuery(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0);
        // sorter3.RenderMeshes(MeshSorter::kOpaque, gfxContext, globals);
        sorter3.RenderMeshesForOcclusion(MeshSorter::kOpaque, gfxContext, globals);
        gfxContext.EndQuery(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0);

        // block.model.Translate(Vector3(50,50,50));
        
        gfxContext.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST));
        gfxContext.GetCommandList()->ResolveQueryData(m_queryHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, 1, m_queryResult, 0);
        gfxContext.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
    }

    // Some systems generate a per-pixel velocity buffer to better track dynamic and skinned meshes.  Everything
    // is static in our scene, so we generate velocity from camera motion and the depth buffer.  A velocity buffer
    // is necessary for all temporal effects (and motion blur).
    MotionBlur::GenerateCameraVelocityBuffer(gfxContext, m_Camera, true);

    TemporalEffects::ResolveImage(gfxContext);

    //ParticleEffectManager::Render(gfxContext, m_Camera, g_SceneColorBuffer, g_SceneDepthBuffer,  g_LinearDepth[FrameIndex]);

    // Until I work out how to couple these two, it's "either-or".
    if (DepthOfField::Enable)
        DepthOfField::Render(gfxContext, m_Camera.GetNearClip(), m_Camera.GetFarClip());
    else
        MotionBlur::RenderObjectBlur(gfxContext, g_VelocityBuffer);
    gfxContext.Finish();
}
