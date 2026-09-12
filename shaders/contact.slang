#include "common.slang"

// Screen-space contact shadows: from every opaque pixel, march the depth
// buffer toward the sun and darken where nearby geometry blocks the ray.
// Multiplied onto the lit HDR image before the atmosphere passes. Meant for
// close rock pairs; occluders far in front are ignored as unrelated geometry.

static const int CONTACT_STEPS = 16;
static const float CONTACT_LENGTH = .6;     // world units the ray searches toward the sun
static const float CONTACT_THICKNESS = .12; // largest depth gap still counted as the same neighbourhood
static const float CONTACT_BIAS = .004;     // plus a slope term, to avoid self-shadowing on the start surface
static const float CONTACT_DARKEST = .25;   // shadow at a touching occluder; farther occluders shade less

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float depth = textures[18].Load(int3(int2(position.xy), 0)).r;
    if (depth >= .999999) return float4(1.0, 1.0, 1.0, 1.0);
    float3 rd = rayDirection(position.xy);
    float3 p = rd * sceneDistance(position.xy, rd);
    float3 light = normalize(root.frame->sun.xyz - p);
    float3 forward = root.frame->forward_exposure.xyz;
    float2 resolution = root.frame->options.xy;
    // Per-pixel, per-frame start offset: the temporal pass averages the banding away.
    float jitter = hash(float3(position.xy, frac(root.frame->camera_time.w * 3.0) * 100.0));
    float shadow = 1.0;
    for (int i = 0; i < CONTACT_STEPS; ++i)
    {
        float t = (float(i) + jitter) / float(CONTACT_STEPS) * CONTACT_LENGTH;
        float3 s = p + light * t;
        float4 clip = mul(root.frame->view_projection, float4(s, 1.0));
        if (clip.w <= 0.0) break;
        float2 uv = clip.xy / clip.w * .5 + .5;
        if (any(uv < 0.0) || any(uv > 1.0)) break;
        float sampleDepth = textures[18].Load(int3(int2(uv * resolution), 0)).r;
        if (sampleDepth >= .999999) continue;
        float gap = dot(s, forward) - linearViewDepth(sampleDepth);
        if (gap > CONTACT_BIAS + dot(s, forward) * .002 && gap < CONTACT_THICKNESS)
        {
            shadow = lerp(CONTACT_DARKEST, 1.0, t / CONTACT_LENGTH);
            break;
        }
    }
    return float4(shadow, shadow, shadow, 1.0);
}
