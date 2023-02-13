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
// Author(s):  James Stanard
//

#include "Common.hlsli"

#ifdef ENABLE_SKINNING
//#undef ENABLE_SKINNING
#endif
StructuredBuffer<InstanceData> gInstanceData: register(t0, space2);

cbuffer GlobalConstants : register(b1)
{
    float4x4 ViewProjMatrix;
}

#ifdef ENABLE_SKINNING
struct Joint
{
    float4x4 PosMatrix;
    float4x3 NrmMatrix; // Inverse-transpose of PosMatrix
};

StructuredBuffer<Joint> Joints : register(t20);
#endif

struct VSInput
{
    float3 position : POSITION;
#ifdef ENABLE_ALPHATEST
    float2 uv0 : TEXCOORD0;
#endif
#ifdef ENABLE_SKINNING
    uint4 jointIndices : BLENDINDICES;
    float4 jointWeights : BLENDWEIGHT;
#endif
};

struct VSOutput
{
    float4 position : SV_POSITION;
#ifdef ENABLE_ALPHATEST
    float2 uv0 : TEXCOORD0;
#endif
};

// [RootSignature(Renderer_RootSig)]
VSOutput main(VSInput vsInput, uint instanceID : SV_InstanceID)
{
    VSOutput vsOutput;
    InstanceData instData = gInstanceData[instanceID];
    float4x4 WorldMatrix =instData.WorldMatrix;
    float4 position = float4(vsInput.position, 1.0);
    float3 worldPos = mul(WorldMatrix, position).xyz;

#ifdef ENABLE_SKINNING
    // I don't like this hack.  The weights should be normalized already, but something is fishy.
    float4 weights = vsInput.jointWeights / dot(vsInput.jointWeights, 1);

    float4x4 skinPosMat =
        Joints[vsInput.jointIndices.x].PosMatrix * weights.x +
        Joints[vsInput.jointIndices.y].PosMatrix * weights.y +
        Joints[vsInput.jointIndices.z].PosMatrix * weights.z +
        Joints[vsInput.jointIndices.w].PosMatrix * weights.w;

    position = mul(skinPosMat, position);

#endif
    vsOutput.position = mul(ViewProjMatrix, float4(worldPos, 1.0));

#ifdef ENABLE_ALPHATEST
    vsOutput.uv0 = vsInput.uv0;
#endif

    return vsOutput;
}
