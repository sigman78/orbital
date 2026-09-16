#pragma once
#include "../scene_shared.h"
#include "../lib/projection.slang"

float3 rayDirection(Frame* frame, float2 pixel)
{
    float2 xy = (pixel - frame->jitter.xy) / frame->options.xy * 2.0 - 1.0;
    return normalize(frame->forward_exposure.xyz + frame->right_tan.xyz * xy.x * frame->right_tan.w * frame->up_aspect.w
                     - frame->up_aspect.xyz * xy.y * frame->right_tan.w);
}

float sceneDistance(Frame* frame, Texture2D<float4> depth, float2 pixel, float3 rd)
{
    float d = depth.Load(int3(int2(pixel), 0)).r;
    if (isBackgroundDepth(d)) return 1e9;
    return linearViewDepth(d) / max(dot(rd, frame->forward_exposure.xyz), 1e-4);
}
