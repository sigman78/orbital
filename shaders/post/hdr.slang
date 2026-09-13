#pragma once
#include "../scene/frame_bindings.slang"

// Temporally resolved HDR input shared by bloom, metering and compositing.
float3 sampleHDR(float2 uv) { return textures[TEX_HISTORY_A].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rgb; }
