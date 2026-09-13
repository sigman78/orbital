#pragma once
#include "../scene/bindings.slang"
#include "../lib/noise.slang"
#include "../lib/sampling.slang"
#include "cloud_geometry.slang"

// The Earth's cloud layer, shared by the surface shader (which draws the
// clouds and shadows the ground) and the atmosphere shader (which shadows the
// air below the clouds, so cloud shadows read over the dark sea as darker
// haze).

static const float CLOUD_DRIFT = .00012; // uv per second; Earth itself turns 1/1600 of a revolution per second

// Cloud coordinate: a slow drift plus a slower, larger swirl that follows it.
float2 cloudUV(float2 uv, float t)
{
    uv.x += t * CLOUD_DRIFT;
    float2 swirl = float2(fbm2(uv * float2(6, 12) + t * .003, 3),
                          fbm2(uv * float2(6, 12) + float2(11.3, 4.7) - t * .002, 3)) - .5;
    return uv + swirl * float2(.006, .004);
}

// Cloud coverage as a shadow caster: read at a coarser footprint for the
// penumbra the sun's disc and in-cloud scattering give, without the fine
// structure, so shadows are softer than the clouds that cast them.
float cloudShadowDensity(float2 uv, float softness)
{
    return pow(saturate(sampleSphereBias(textures[TEX_EARTH_CLOUDS], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv, softness).a), 1.3);
}

// The same from an explicit mip, for passes without screen-space derivatives of the cloud coordinate.
float cloudShadowDensityLevel(float2 uv, float level)
{
    return pow(saturate(textures[TEX_EARTH_CLOUDS].SampleLevel(samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv, level).a), 1.3);
}
