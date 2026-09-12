#include "common.slang"

// Temporal anti-aliasing over the jittered HDR frame. History is reprojected
// through the previous frame's projection and camera delta, clipped to the
// current 3x3 neighbourhood in YCoCg, and blended with luminance weights so
// bright specks settle instead of flickering. Depth only rejects history when
// both frames saw a surface and disagree: a pixel that flips between a thin
// rock and the sky keeps its history and lets the colour clip decide.
// Frame.quality.x below 1 turns the pass into a copy.

static const float HISTORY_WEIGHT = .95;      // at rest
static const float HISTORY_WEIGHT_MOVING = .80; // at one pixel of motion per frame and beyond
static const float CLIP_SIGMA = 1.5;          // variance clip half-width, in standard deviations

float3 toYCoCg(float3 c) { return float3(.25 * c.r + .5 * c.g + .25 * c.b, .5 * c.r - .5 * c.b, -.25 * c.r + .5 * c.g - .25 * c.b); }
float3 fromYCoCg(float3 c) { return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }

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
float4 sampleHistory(float2 uv, float2 size)
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
    float4 result = textures[17].SampleLevel(samplers[0], float2(tc12.x, tc0.y), 0.0) * w12.x * w0.y
                  + textures[17].SampleLevel(samplers[0], float2(tc0.x, tc12.y), 0.0) * w0.x * w12.y
                  + textures[17].SampleLevel(samplers[0], float2(tc12.x, tc12.y), 0.0) * w12.x * w12.y
                  + textures[17].SampleLevel(samplers[0], float2(tc3.x, tc12.y), 0.0) * w3.x * w12.y
                  + textures[17].SampleLevel(samplers[0], float2(tc12.x, tc3.y), 0.0) * w12.x * w3.y;
    float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max(result / weight, float4(0.0, 0.0, 0.0, 0.0));
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    int2 size = int2(root.frame.options.xy);
    int2 pixel = clamp(int2(position.xy), int2(0, 0), size - 1);
    float2 uv = (float2(pixel) + .5) / float2(size);
    float3 current = textures[0].Load(int3(pixel, 0)).rgb;
    float deviceDepth = textures[18].Load(int3(pixel, 0)).r;
    bool sky = deviceDepth >= .999999;
    float currentZ = sky ? FAR_Z : linearViewDepth(deviceDepth);
    if (root.frame.previous_camera_delta.w < .5 || root.frame.quality.x < .5) return float4(current, currentZ);

    float3 rd = rayDirection(position.xy);
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
    float2 previousUV = previousClip.xy / max(previousClip.w, 1e-5) * .5 + .5;
    bool outside = any(previousUV < float2(0.0, 0.0)) || any(previousUV > float2(1.0, 1.0));
    if (behind || outside || (!sky && predictedPreviousZ <= 0.0)) return float4(current, currentZ);

    float4 history = sampleHistory(previousUV, float2(size));
    // A disocclusion: both frames saw surfaces at clearly different depths.
    bool historySky = history.a >= FAR_Z * .999;
    if (!sky && !historySky) {
        float tolerance = .05 * max(predictedPreviousZ, 0.0) + .1;
        if (abs(history.a - predictedPreviousZ) > tolerance) return float4(current, currentZ);
    }

    float3 mean = float3(0.0, 0.0, 0.0), meanSquare = float3(0.0, 0.0, 0.0);
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        int2 q = clamp(pixel + int2(x, y), int2(0, 0), size - 1);
        float3 c = toYCoCg(textures[0].Load(int3(q, 0)).rgb);
        mean += c;
        meanSquare += c * c;
    }
    mean /= 9.0;
    float3 sigma = sqrt(max(meanSquare / 9.0 - mean * mean, float3(0.0, 0.0, 0.0)));
    float3 boxMin = mean - sigma * CLIP_SIGMA, boxMax = mean + sigma * CLIP_SIGMA;
    float3 clippedHistory = fromYCoCg(clipToBox(boxMin, boxMax, toYCoCg(history.rgb)));

    float motionPixels = length((previousUV - uv) * float2(size));
    float historyWeight = lerp(HISTORY_WEIGHT, HISTORY_WEIGHT_MOVING, smoothstep(.25, 1.0, motionPixels));
    // Luminance-weighted blend: bright specks count less, so they settle instead of flickering.
    float currentWeight = (1.0 - historyWeight) / (1.0 + dot(current, float3(.2126, .7152, .0722)));
    float previousWeight = historyWeight / (1.0 + dot(clippedHistory, float3(.2126, .7152, .0722)));
    float3 blended = (current * currentWeight + clippedHistory * previousWeight) / (currentWeight + previousWeight);
    return float4(blended, currentZ);
}
