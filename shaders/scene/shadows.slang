#pragma once
#include "bindings.slang"
#include "frame_records.slang"
#include "../lib/noise.slang"
#include "../belt/belt.slang"

float localShadow(Frame* frame, float3 point, float3 normal)
{
    float4 light = mul(shadowProjection(frame), float4(point + normal * 0.015, 1.0));
    float3 coord = light.xyz / light.w;
    float2 uv = coord.xy * 0.5 + 0.5;
    if (any(uv < 0.001) || any(uv > 0.999) || coord.z < 0.0 || coord.z > 1.0) return 1.0;
    uint width, height;
    textures[TEX_SHADOW_MAP].GetDimensions(width, height);
    float2 texel = 1.0 / float2(width, height);
    float result = 0.0;
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)
        result += textures[TEX_SHADOW_MAP].SampleLevel(samplers[SAMPLER_CLAMP], uv + float2(x, y) * texel, 0.0).r >= coord.z - 0.0003 ? 1.0 : 0.0;
    return result / 9.0;
}

float beltShadow(Frame* frame, float3 point)
{
    BeltParams belt = beltParams(frame);
    if (belt.density <= 0.0) return 1.0;
    float3 light = normalize(lightParams(frame).position - point);
    float denominator = dot(light, belt.normal);
    if (abs(denominator) < 1e-4) return 1.0;
    float t = dot(belt.centre - point, belt.normal) / denominator;
    if (t <= 0.0) return 1.0;
    float3 q = point + light * t - belt.centre;
    float r = length(q);
    float density = beltRingDensity((r - belt.inner) / (belt.outer - belt.inner)); // the profile carries the edge tapers
    float grain = .8 + .2 * noise(q * .5);
    return 1.0 - belt.density * .585 * density * grain;
}
