#include "../scene_shared.h"
#include "../scene/bindings.slang"
#include "../scene/frame_records.slang"
#include "../lib/color.slang"

// The exposure meter as a luminance histogram accumulated over sixteen frames.
// Every frame bins one sixteenth of a four pixel tap grid over the resolved HDR
// image (the rows whose index matches the phase), so the work is a few
// microseconds a frame with no periodic bump; the CPU reads the full histogram
// once per cycle and the buffer is zeroed before the next. Each tap adds its
// centre weight to the bin of its luminance, with the sun's glare added as the
// composite draws it, and keeps the brightest luminance seen.

[[vk::push_constant]] ConstantBuffer<MeterRoot> root;

static const float METER_STRIDE = 4.0;        // pixels between taps
static const float METER_CENTRE_WEIGHT = .6;  // fraction of centre-biased metering
static const float METER_WEIGHT_SCALE = 1024.0; // fixed-point weight per tap

// The temporally resolved HDR image, as hdr.slang samples it for the composite.
float3 resolvedHDR(Frame* frame, float2 uv)
{
    MoteParams mote = moteParams(frame);
    ViewParams view = viewParams(frame);
    float3 color = textures[TEX_HISTORY_A].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rgb;
    if (mote.streaks > 0.0)
        color += textures[TEX_HDR].SampleLevel(samplers[SAMPLER_CLAMP], uv + view.jitter / view.size, 0.0).rgb;
    return color;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    Frame* frame = root.frame;
    ViewParams view = viewParams(frame);
    ScreenSun sunScreen = screenSun(frame);
    LensParams lens = lensParams(frame);
    float2 resolution = view.size;
    uint2 taps = uint2(ceil(resolution / METER_STRIDE));
    // The phases take the rows in bit-reversed order (0, 8, 4, 12, ...) rather than top
    // to bottom, so a cycle cut short by a change of view has sampled the whole frame
    // evenly instead of one contiguous band of it.
    uint slice = reversebits(root.phase) >> (32 - 4); // ORBITAL_METER_PHASES is 16
    uint row = thread.y * ORBITAL_METER_PHASES + slice; // this phase's rows of the tap grid
    if (thread.x >= taps.x || row >= taps.y) return;
    float2 uv = (float2(thread.x, row) + .5) * METER_STRIDE / resolution;
    float lum = luma709(resolvedHDR(frame, uv));
    // The sun's glare over this tap, as composite.slang draws it.
    float2 p = uv * 2.0 - 1.0;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float vis = float(sunScreen.visible) * textures[TEX_SUN_VISIBILITY].Load(int3(0, 0, 0)).r;
    float dist = length((p - sunScreen.position) * aspect);
    lum += luma709(float3(1.0, .6, .3)) * vis * .014 * lens.glare / (dist * dist + .012) * exp(-dist * 1.5);
    float weight = 1.0 - METER_CENTRE_WEIGHT * smoothstep(0.0, 1.0, length(p));
    float position = (log2(max(lum, 1e-6)) - ORBITAL_METER_LOG_MIN) / ORBITAL_METER_LOG_RANGE;
    uint bin = uint(clamp(position * float(ORBITAL_METER_BINS), 0.0, float(ORBITAL_METER_BINS - 1)));
    MeterHistogram* histogram = root.histogram;
    histogram->bins[bin].add(uint(weight * METER_WEIGHT_SCALE + .5));
    histogram->peak.max(asuint(max(lum, 0.0))); // positive floats order as their bits do
}
