#pragma once
#include "../scene_shared.h"
#include "../scene/frame_records.slang"
#include "../lib/projection.slang"

// Opaque geometry blocks fully; billboards attenuate by their actual alpha.
float sunPixelVisibility(Texture2D<float4> depth, Texture2D<float4> splats, int2 pixel)
{
    float open = isBackgroundDepth(depth.Load(int3(pixel, 0)).r) ? 1.0 : 0.0;
    return open * (1.0 - saturate(splats.Load(int3(pixel, 0)).a));
}

// Fraction of the solar disc not covered by scene geometry: depth taps over
// the disc's screen footprint, so rocks and moons dim the glare and the
// flare as they cross the sun (the planets are handled analytically).
float sunDepthVisibility(Frame* frame, Texture2D<float4> depth, Texture2D<float4> splats, float2 sun, float2 aspect)
{
    ViewParams view = viewParams(frame);
    LightParams light = lightParams(frame);
    float2 uv = sun * .5 + .5 + view.jitter / view.size;
    if (any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    float2 radius = light.discRadius / view.tanHalfFov / aspect * .5; // disc radius in uv
    float open = 0.0;
    // Evaluated once into a 1x1 target, not once per tone-mapped pixel. Dense
    // fixed disc samples avoid the old 1/12 jumps as thin rocks cross the sun.
    static const int samples = 256;
    int2 size = int2(view.size);
    for (int i = 0; i < samples; ++i)
    {
        float r = sqrt((float(i) + .5) / float(samples)), a = float(i) * 2.399963;
        float2 tap = clamp(uv + float2(cos(a), sin(a)) * r * radius, 0.0, 1.0);
        float2 pixel = tap * float2(size) - .5;
        int2 first = int2(floor(pixel));
        float2 f = frac(pixel);
        // Filter visibility, not depth: thresholding interpolated depth makes
        // even a tiny contribution from a foreground rock close the whole tap.
        float a00 = sunPixelVisibility(depth, splats, clamp(first, int2(0, 0), size - 1));
        float a10 = sunPixelVisibility(depth, splats, clamp(first + int2(1, 0), int2(0, 0), size - 1));
        float a01 = sunPixelVisibility(depth, splats, clamp(first + int2(0, 1), int2(0, 0), size - 1));
        float a11 = sunPixelVisibility(depth, splats, clamp(first + int2(1, 1), int2(0, 0), size - 1));
        open += lerp(lerp(a00, a10, f.x), lerp(a01, a11, f.x), f.y);
    }
    return open / float(samples);
}
