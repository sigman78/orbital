#pragma once
#include "galactic.slang"
#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"

// Original display-space source, using the same radiance convention as the fit.
float3 galaxyOriginal(float3 ray)
{
    float2 uv = galacticUV(galacticDirection(ray));
    float2 dx = ddx(uv), dy = ddy(uv);
    dx.x -= round(dx.x);
    dy.x -= round(dy.x);
    float3 source = textures[TEX_GALAXY_ORIGINAL].SampleGrad(samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv, dx, dy).rgb;
    return .1 * pow(source * source, skyParams(root.frame).bandContrast);
}

// Matches tools/bake-galaxy-layers.py: global RGB plus RGB clouds on a +/-45 degree
// strip, attenuated by a scalar filament mask. Colour maps store square-root radiance.
float3 galaxyLayers(float3 ray)
{
    SkyParams sky = skyParams(root.frame);
    float2 uv = galacticUV(galacticDirection(ray));
    float2 dx = ddx(uv), dy = ddy(uv);
    dx.x -= round(dx.x); // longitude wrap must not select a coarse mip at the seam
    dy.x -= round(dy.x);
    float2 bandUV = float2(uv.x, (uv.y - .25) * 2.0);
    float2 bandDx = dx * float2(1.0, 2.0), bandDy = dy * float2(1.0, 2.0);
    float band = 1.0 - smoothstep(.20, .25, abs(uv.y - .5));
    float3 low = textures[TEX_GALAXY_LOW].SampleGrad(samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv, dx, dy).rgb;
    float3 clouds = textures[TEX_GALAXY_CLOUDS].SampleGrad(samplers[SAMPLER_WRAP_U_ANISOTROPIC], bandUV, bandDx, bandDy).rgb;
    float dark = textures[TEX_GALAXY_FILAMENTS].SampleGrad(samplers[SAMPLER_WRAP_U_ANISOTROPIC], bandUV, bandDx, bandDy).r;
    float3 light = sky.lowGain * low * low + sky.cloudGain * band * clouds * clouds;
    float3 radiance = light * saturate(1.0 - sky.filamentGain * band * dark);
    return .1 * pow(max(radiance, 0.0), sky.bandContrast);
}
