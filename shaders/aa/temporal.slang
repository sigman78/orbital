#include "../scene/frame_bindings.slang"
#include "../scene/view.slang"
#include "../lib/color.slang"

// Resolve jittered scene samples onto a stable output/history grid. Previous
// projection remains jittered for other consumers; remove its displacement here.
// Splat metadata carries coverage and coverage-weighted depth, never scene depth.
// Coverage-aware history validation is a separate follow-up to this coordinate fix.

static const float HISTORY_WEIGHT = .95;      // at rest
static const float HISTORY_WEIGHT_MOVING = .60; // at one pixel of motion per frame and beyond; shorter keeps flight sharper
static const float CLIP_SIGMA = 1.5;          // variance clip half-width at rest, in standard deviations
static const float CLIP_SIGMA_MOVING = .75;   // and at CLIP_MOTION_PIXELS of motion and beyond: a tighter box drops the copies a fast rock leaves behind
static const float CLIP_MOTION_PIXELS = 3.0;
static const float SPLAT_WEIGHT_MOVING = .3;  // splat pixels at SPLAT_MOTION_PIXELS of motion and beyond: already filtered, so a short history keeps them sharp
static const float SPLAT_MOTION_PIXELS = 4.0;
static const float SPLAT_CLIP_SIGMA = 3.0;    // splat history is clipped only loosely: its own filtering settles instead of twinkling

// Moves the history colour to the nearest point of the neighbourhood box along
// the line toward the box centre, which keeps hue better than a clamp.
float3 clipToBox(float3 boxMin, float3 boxMax, float3 history)
{
    float3 centre = .5 * (boxMax + boxMin), extent = .5 * (boxMax - boxMin) + 1e-5;
    float3 offset = history - centre;
    float3 unit = abs(offset / extent);
    float largest = max(unit.x, max(unit.y, unit.z));
    return largest > 1.0 ? centre + offset / largest : history;
}

// Five-tap Catmull-Rom history fetch: sharper than bilinear over many frames.
float4 reconstruct(Texture2D<float4> source, float2 uv, float2 size)
{
    float2 texel = 1.0 / size;
    float2 position = uv * size;
    float2 centre = floor(position - .5) + .5;
    float2 f = position - centre;
    float2 w0 = f * (-.5 + f * (1.0 - .5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-.5 + .5 * f);
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / w12;
    float2 tc0 = (centre - 1.0) * texel, tc3 = (centre + 2.0) * texel, tc12 = (centre + offset12) * texel;
    float4 result = source.SampleLevel(samplers[SAMPLER_CLAMP], float2(tc12.x, tc0.y), 0.0) * w12.x * w0.y
                  + source.SampleLevel(samplers[SAMPLER_CLAMP], float2(tc0.x, tc12.y), 0.0) * w0.x * w12.y
                  + source.SampleLevel(samplers[SAMPLER_CLAMP], float2(tc12.x, tc12.y), 0.0) * w12.x * w12.y
                  + source.SampleLevel(samplers[SAMPLER_CLAMP], float2(tc3.x, tc12.y), 0.0) * w3.x * w12.y
                  + source.SampleLevel(samplers[SAMPLER_CLAMP], float2(tc12.x, tc3.y), 0.0) * w12.x * w3.y;
    float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max(result / weight, float4(0.0, 0.0, 0.0, 0.0));
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    int2 size = int2(root.frame.options.xy);
    int2 pixel = clamp(int2(position.xy), int2(0, 0), size - 1);
    float2 uv = (float2(pixel) + .5) / float2(size);
    float2 rasterPosition = position.xy + root.frame.jitter.xy;
    float2 rasterUV = rasterPosition / float2(size);
    float3 current = reconstruct(textures[TEX_HDR], rasterUV, float2(size)).rgb;
    pixel = clamp(int2(rasterPosition), int2(0, 0), size - 1);
    float deviceDepth = textures[TEX_DEPTH].Load(int3(pixel, 0)).r;
    float4 metadata = textures[TEX_SPLAT_MASK].Load(int3(pixel, 0));
    bool splat = metadata.a > 1e-5;
    bool sky = isBackgroundDepth(deviceDepth) && !splat;
    float currentZ = splat ? metadata.r / metadata.a : (sky ? FAR_Z : linearViewDepth(deviceDepth));
    if (root.frame.previous_camera_delta.w < .5 || root.frame.quality.x < .5) return float4(current, currentZ);

    float3 rd = rayDirection(root.frame, rasterPosition);
    float4 previousClip;
    float predictedPreviousZ = FAR_Z;
    if (sky) {
        previousClip = mul(root.frame.previous_projection, float4(rd, 0.0));
    } else {
        float forwardCosine = max(dot(rd, root.frame.forward_exposure.xyz), 1e-4);
        float3 currentPosition = rd * (currentZ / forwardCosine);
        float3 previousPosition = currentPosition + root.frame.previous_camera_delta.xyz;
        predictedPreviousZ = dot(previousPosition, root.frame.previous_forward.xyz);
        previousClip = mul(root.frame.previous_projection, float4(previousPosition, 1.0));
    }
    bool behind = previousClip.w <= 1e-5;
    float2 previousUV = previousClip.xy / max(previousClip.w, 1e-5) * .5 + .5
                      - root.frame.jitter.zw / float2(size);
    bool outside = any(previousUV < float2(0.0, 0.0)) || any(previousUV > float2(1.0, 1.0));
    if (behind || outside || (!sky && predictedPreviousZ <= 0.0)) return float4(current, currentZ);

    float4 history = reconstruct(textures[TEX_HISTORY_B], previousUV, float2(size));
    // Depth is metadata, not colour: Catmull-Rom can overshoot at silhouettes
    // and invent a surface between foreground and background. Compare against
    // the unfiltered depths within one pixel of the reprojected sample. This
    // covers the shift in coverage between jitter phases while rejecting disocclusions
    // where every history sample lies clearly in front of or behind the surface.
    if (!sky && !splat) {
        int2 historyPixel = int2(previousUV * float2(size));
        float minDepth = FAR_Z, maxDepth = 0.0;
        for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
            int2 q = clamp(historyPixel + int2(x, y), int2(0, 0), size - 1);
            float z = textures[TEX_HISTORY_B].Load(int3(q, 0)).a;
            minDepth = min(minDepth, z);
            maxDepth = max(maxDepth, z);
        }
        float tolerance = .05 * max(predictedPreviousZ, 0.0) + .1;
        if (predictedPreviousZ < minDepth - tolerance || predictedPreviousZ > maxDepth + tolerance)
            return float4(current, currentZ);
    }

    float3 mean = float3(0.0, 0.0, 0.0), meanSquare = float3(0.0, 0.0, 0.0);
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        float2 q = rasterUV + float2(x, y) / float2(size);
        float3 c = toYCoCg(textures[TEX_HDR].SampleLevel(samplers[SAMPLER_CLAMP], q, 0.0).rgb);
        mean += c;
        meanSquare += c * c;
    }
    mean /= 9.0;
    float3 sigma = sqrt(max(meanSquare / 9.0 - mean * mean, float3(0.0, 0.0, 0.0)));
    // The box tightens with motion: at speed a rock crosses pixels whose
    // neighbourhood still holds bright rock, and a wide box let its old copies
    // through as a trail. Splat pixels were never clipped and trailed worst;
    // they now get a loose box, which still settles their filtered footprints.
    float motionPixels = length((previousUV - uv) * float2(size));
    float clipSigma = splat ? SPLAT_CLIP_SIGMA
                            : lerp(CLIP_SIGMA, CLIP_SIGMA_MOVING, smoothstep(.5, CLIP_MOTION_PIXELS, motionPixels));
    float3 boxMin = mean - sigma * clipSigma, boxMax = mean + sigma * clipSigma;
    float3 clippedHistory = fromYCoCg(clipToBox(boxMin, boxMax, toYCoCg(history.rgb)));

    float historyWeight = splat ? lerp(HISTORY_WEIGHT, SPLAT_WEIGHT_MOVING, smoothstep(.25, SPLAT_MOTION_PIXELS, motionPixels))
                              : lerp(HISTORY_WEIGHT, HISTORY_WEIGHT_MOVING, smoothstep(.25, 1.0, motionPixels));
    // Luminance-weighted blend: bright specks count less, so they settle instead of flickering.
    float currentWeight = (1.0 - historyWeight) / (1.0 + luma709(current));
    float previousWeight = historyWeight / (1.0 + luma709(clippedHistory));
    float3 blended = (current * currentWeight + clippedHistory * previousWeight) / (currentWeight + previousWeight);
    return float4(blended, currentZ);
}
