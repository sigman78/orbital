#include "common.slang"

float3 filteredStars(float3 p)
{
    // Gaussian pixel convolution: derivative variance broadens undersampled
    // points while the area ratio keeps their integrated screen energy stable.
    const float baseVariance = 1.0 / 220.0;
    float3 px = ddx(p), py = ddy(p);
    float variance = baseVariance + (dot(px, px) + dot(py, py)) / 12.0;
    variance = min(variance, .12);
    float energyScale = baseVariance / variance;
    float3 first = floor(p - .5) + .5;
    float3 result = float3(0.0, 0.0, 0.0);
    for (int z = 0; z < 2; ++z) for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
    {
        float3 centre = first + float3(float(x), float(y), float(z));
        float3 cell = centre - .5;
        float3 delta = p - centre;
        float id = hash(cell), r = length(delta);
        // Compact support is zero before a neighbour leaves this search set,
        // preventing a residual Gaussian tail from popping at cell boundaries.
        float kernel = exp(-dot(delta, delta) / (2.0 * variance)) * (1.0 - smoothstep(.72, .95, r));
        float3 tint = lerp(float3(.65, .78, 1.0), float3(1.0, .83, .62), hash(cell + 7.0));
        result += tint * kernel * step(.992, id);
    }
    return result * energyScale * 2.2;
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float3 rd = rayDirection(position.xy);
    float galactic = pow(max(0.0, 1.0 - abs(dot(rd, normalize(float3(.3, .8, .4))))), 24.0);
    float3 color = float3(.0005, .0007, .0012) + float3(.008, .009, .014) * galactic * (.4 + fbm(rd * 14.0));
    color += filteredStars(rd * 480.0);
    float3 sunDir = normalize(root.frame.sun.xyz - root.frame.camera_time.xyz);
    float angle = acos(clamp(dot(rd, sunDir), -1.0, 1.0));
    float disc = 1.0 - smoothstep(.006, .008, angle);
    color += float3(1.0, .85, .59) * disc * 35.0;
    color += float3(1.0, .57, .25) * .06 * exp(-angle * 34.0);
    return float4(color, 1.0);
}
