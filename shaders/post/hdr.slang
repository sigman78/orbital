#pragma once
#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"

// Temporally resolved HDR input shared by bloom, metering and compositing.
float3 sampleHDR(float2 uv)
{
    MoteParams mote = moteParams(root.frame);
    ViewParams view = viewParams(root.frame);
    float3 color = textures[TEX_HISTORY_A].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rgb;
    // Transient motion streaks live in the scene HDR storage once the resolve has consumed
    // it, or in the idle history target while the temporal pass is off and the scene is the
    // resolved image. Their raster grid is jittered; sample onto the stable post-processing
    // grid. At rest this uniform branch also prevents reading last frame's streaks.
    if (mote.streaks > 0.0)
    {
        float2 streakUv = uv + view.jitter / view.size;
        color += temporalParams(root.frame).enabled ? textures[TEX_HDR].SampleLevel(samplers[SAMPLER_CLAMP], streakUv, 0.0).rgb
                                                    : textures[TEX_HISTORY_B].SampleLevel(samplers[SAMPLER_CLAMP], streakUv, 0.0).rgb;
    }
    return color;
}
