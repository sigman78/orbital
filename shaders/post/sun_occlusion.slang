#pragma once
#include "../scene_shared.h"

// Fraction of the solar disc not covered by scene geometry: depth taps over
// the disc's screen footprint, so rocks and moons dim the glare and the
// flare as they cross the sun (the planets are handled analytically).
float sunDepthVisibility(Frame* frame, Texture2D<float4> depth, float2 sun, float2 aspect)
{
    float2 uv = sun * .5 + .5 + frame->jitter.xy / frame->options.xy;
    if (any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    float2 radius = frame->sun_disc.x / frame->right_tan.w / aspect * .5; // disc radius in uv
    float open = 0.0;
    // Evaluated once into a 1x1 target, not once per tone-mapped pixel. Dense
    // fixed disc samples avoid the old 1/12 jumps as thin rocks cross the sun.
    static const int samples = 256;
    int2 size = int2(frame->options.xy);
    for (int i = 0; i < samples; ++i)
    {
        float r = sqrt((float(i) + .5) / float(samples)), a = float(i) * 2.399963;
        float2 tap = clamp(uv + float2(cos(a), sin(a)) * r * radius, 0.0, 1.0);
        float2 pixel = tap * float2(size) - .5;
        int2 first = int2(floor(pixel));
        float2 f = frac(pixel);
        // Filter visibility, not depth: thresholding interpolated depth makes
        // even a tiny contribution from a foreground rock close the whole tap.
        float a00 = step(.999999, depth.Load(int3(clamp(first, int2(0, 0), size - 1), 0)).r);
        float a10 = step(.999999, depth.Load(int3(clamp(first + int2(1, 0), int2(0, 0), size - 1), 0)).r);
        float a01 = step(.999999, depth.Load(int3(clamp(first + int2(0, 1), int2(0, 0), size - 1), 0)).r);
        float a11 = step(.999999, depth.Load(int3(clamp(first + int2(1, 1), int2(0, 0), size - 1), 0)).r);
        open += lerp(lerp(a00, a10, f.x), lerp(a01, a11, f.x), f.y);
    }
    return open / float(samples);
}
