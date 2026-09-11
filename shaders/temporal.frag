#version 460
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) out vec4 outColor;

const float NEAR_Z = .02;
const float FAR_Z = 2000.0;

float linearViewDepth(float deviceDepth) {
    return (NEAR_Z * FAR_Z) /
           max(FAR_Z - deviceDepth * (FAR_Z - NEAR_Z), 1e-5);
}

void main() {
    ivec2 size = ivec2(root.frame.options.xy);
    ivec2 pixel = clamp(ivec2(gl_FragCoord.xy), ivec2(0), size - 1);
    vec2 uv = (vec2(pixel) + .5) / vec2(size);
    vec3 current = texelFetch(sampler2D(textures[0], samplers[0]), pixel, 0).rgb;
    float deviceDepth = texelFetch(sampler2D(textures[18], samplers[0]), pixel, 0).r;
    bool sky = deviceDepth >= .999999;
    float currentZ = sky ? FAR_Z : linearViewDepth(deviceDepth);

    if (root.frame.previous_camera_delta.w < .5) {
        outColor = vec4(current, currentZ);
        return;
    }

    vec3 rd = rayDirection(gl_FragCoord.xy);
    vec4 previousClip;
    float predictedPreviousZ = FAR_Z;
    if (sky) {
        // A direction (w=0) removes camera translation while retaining the
        // previous view rotation, keeping distant stars temporally stable.
        previousClip = root.frame.previous_projection * vec4(rd, 0.0);
    } else {
        float forwardCosine = max(dot(rd, root.frame.forward_exposure.xyz), 1e-4);
        vec3 position = rd * (currentZ / forwardCosine);
        vec3 previousPosition = position + root.frame.previous_camera_delta.xyz;
        predictedPreviousZ = dot(previousPosition, root.frame.previous_forward.xyz);
        previousClip = root.frame.previous_projection * vec4(previousPosition, 1.0);
    }

    bool behind = previousClip.w <= 1e-5;
    vec2 previousUV = previousClip.xy / max(previousClip.w, 1e-5) * .5 + .5;
    bool outside = any(lessThan(previousUV, vec2(0.0))) ||
                   any(greaterThan(previousUV, vec2(1.0)));
    if (behind || outside || (!sky && predictedPreviousZ <= 0.0)) {
        outColor = vec4(current, currentZ);
        return;
    }

    vec4 history = texture(sampler2D(textures[17], samplers[0]), previousUV);
    if (!sky) {
        float depthTolerance = .02 * max(predictedPreviousZ, 0.0) + .05;
        if (abs(history.a - predictedPreviousZ) > depthTolerance) {
            outColor = vec4(current, currentZ);
            return;
        }
    }

    vec3 neighbourhoodMin = vec3(1e20);
    vec3 neighbourhoodMax = vec3(-1e20);
    vec3 mean = vec3(0.0);
    vec3 meanSquare = vec3(0.0);
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        ivec2 samplePixel = clamp(pixel + ivec2(x, y), ivec2(0), size - 1);
        vec3 sampleColor = texelFetch(sampler2D(textures[0], samplers[0]), samplePixel, 0).rgb;
        neighbourhoodMin = min(neighbourhoodMin, sampleColor);
        neighbourhoodMax = max(neighbourhoodMax, sampleColor);
        mean += sampleColor;
        meanSquare += sampleColor * sampleColor;
    }
    mean /= 9.0;
    vec3 sigma = sqrt(max(meanSquare / 9.0 - mean * mean, vec3(0.0)));

    // The min/max envelope retains isolated HDR stars and tiny bright bodies;
    // a generous variance envelope prevents unrelated saturated neighbours from
    // dragging history color across high-contrast planetary edges.
    vec3 varianceMin = mean - sigma * 2.25;
    vec3 varianceMax = mean + sigma * 2.25;
    vec3 clipMin = max(neighbourhoodMin, varianceMin);
    vec3 clipMax = min(neighbourhoodMax, varianceMax);
    vec3 extent = max(clipMax - clipMin, vec3(0.0));
    clipMin -= extent * .08 + vec3(.0005);
    clipMax += extent * .08 + vec3(.0005);
    vec3 clippedHistory = clamp(history.rgb, clipMin, clipMax);

    float motionPixels = length((previousUV - uv) * vec2(size));
    float historyWeight = mix(.92, .70, smoothstep(.25, 1.5, motionPixels));
    // Fast exposure/lighting changes should converge rather than leave a bright
    // trail, while small subpixel differences retain enough history to settle.
    float currentLuma = dot(current, vec3(.2126, .7152, .0722));
    float historyLuma = dot(clippedHistory, vec3(.2126, .7152, .0722));
    float relativeChange = abs(historyLuma - currentLuma) /
                           max(max(historyLuma, currentLuma), .02);
    historyWeight *= 1.0 - .22 * smoothstep(.25, 1.0, relativeChange);

    outColor = vec4(mix(current, clippedHistory, historyWeight), currentZ);
}
