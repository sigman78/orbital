#pragma once
#include "../scene_shared.h"
#include "frame_records.slang"
#include "../lib/projection.slang"

float3 rayDirection(Frame* frame, float2 pixel)
{
    ViewParams view = viewParams(frame);
    float2 xy = (pixel - view.jitter) / view.size * 2.0 - 1.0;
    return normalize(view.forward + view.right * xy.x * view.tanHalfFov * view.aspect
                     - view.up * xy.y * view.tanHalfFov);
}

float sceneDistance(Frame* frame, Texture2D<float4> depth, float2 pixel, float3 rd)
{
    float d = depth.Load(int3(int2(pixel), 0)).r;
    if (isBackgroundDepth(d)) return 1e9;
    return linearViewDepth(d) / max(dot(rd, viewParams(frame).forward), 1e-4);
}
