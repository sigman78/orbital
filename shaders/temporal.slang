#include "common.slang"

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
    if (root.frame.previous_camera_delta.w < .5) return float4(current, currentZ);

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

    float4 history = textures[17].SampleLevel(samplers[0], previousUV, 0.0);
    if (!sky) {
        float tolerance = .02 * max(predictedPreviousZ, 0.0) + .05;
        if (abs(history.a - predictedPreviousZ) > tolerance) return float4(current, currentZ);
    }

    float3 neighbourhoodMin = float3(1e20, 1e20, 1e20);
    float3 neighbourhoodMax = float3(-1e20, -1e20, -1e20);
    float3 mean = float3(0.0, 0.0, 0.0);
    float3 meanSquare = float3(0.0, 0.0, 0.0);
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        int2 q = clamp(pixel + int2(x, y), int2(0, 0), size - 1);
        float3 c = textures[0].Load(int3(q, 0)).rgb;
        neighbourhoodMin = min(neighbourhoodMin, c);
        neighbourhoodMax = max(neighbourhoodMax, c);
        mean += c;
        meanSquare += c * c;
    }
    mean /= 9.0;
    float3 sigma = sqrt(max(meanSquare / 9.0 - mean * mean, float3(0.0, 0.0, 0.0)));
    float3 clipMin = max(neighbourhoodMin, mean - sigma * 2.25);
    float3 clipMax = min(neighbourhoodMax, mean + sigma * 2.25);
    float3 extent = max(clipMax - clipMin, float3(0.0, 0.0, 0.0));
    clipMin -= extent * .08 + float3(.0005, .0005, .0005);
    clipMax += extent * .08 + float3(.0005, .0005, .0005);
    float3 clippedHistory = clamp(history.rgb, clipMin, clipMax);

    float motionPixels = length((previousUV - uv) * float2(size));
    float historyWeight = lerp(.92, .70, smoothstep(.25, 1.5, motionPixels));
    float currentLuma = dot(current, float3(.2126, .7152, .0722));
    float historyLuma = dot(clippedHistory, float3(.2126, .7152, .0722));
    float relativeChange = abs(historyLuma - currentLuma) / max(max(historyLuma, currentLuma), .02);
    historyWeight *= 1.0 - .22 * smoothstep(.25, 1.0, relativeChange);
    return float4(lerp(current, clippedHistory, historyWeight), currentZ);
}
