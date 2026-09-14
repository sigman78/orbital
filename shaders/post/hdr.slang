#pragma once
#include "../scene/frame_bindings.slang"

// Temporally resolved HDR input shared by bloom, metering and compositing.
float3 sampleHDR(float2 uv)
{
    float3 color = textures[TEX_HISTORY_A].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rgb;
    // Scene HDR storage is reused after resolve for transient motion streaks.
    // Their raster grid is jittered; sample onto the stable post-processing grid.
    // At rest this uniform branch also prevents reading last frame's streaks.
    if (root.frame.camera_cell.w > 0.0)
        color += textures[TEX_HDR].SampleLevel(samplers[SAMPLER_CLAMP],
                   uv + root.frame.jitter.xy / root.frame.options.xy, 0.0).rgb;
    return color;
}
