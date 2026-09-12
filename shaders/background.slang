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
        // With the catalogue drawing the naked-eye stars, these stand for the fainter
        // ones, magnitudes 6.5 to 9, on the same law the catalogue follows (counts
        // growing 2.5 times per magnitude, so most sit at the faint end); flux and
        // peak match the star pass's scale, which puts a magnitude 6.5 star near .007.
        float peak = 2.2;
        if (root.frame.stars.x > .5)
        {
            float magnitude = log(385.0 + hash(cell + 3.0) * 3430.0) / log(2.5);
            peak = 3.0 * pow(10.0, -.4 * magnitude) * root.frame.stars.y;
        }
        result += tint * kernel * step(.992, id) * peak;
    }
    return result * energyScale;
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float3 rd = rayDirection(position.xy);
    // The band lies on the real galactic plane: the pole at RA 192.86, Dec +27.13 in the sky frame the catalogue uses.
    float galactic = pow(max(0.0, 1.0 - abs(dot(rd, float3(-.868, .456, -.198)))), 24.0);
    float3 color = float3(.0005, .0007, .0012) + float3(.008, .009, .014) * galactic * (.4 + fbm(rd * 14.0));
    color += filteredStars(rd * 480.0);
    // The solar disc: its edge softened to the pixel footprint so it does not
    // alias from afar, and limb-darkened (I = 1 - u (1 - mu)); the brightness
    // is normalised so the darkening does not change the disc's total light.
    float3 sunDir = normalize(root.frame.sun.xyz - root.frame.camera_time.xyz);
    float angle = acos(clamp(dot(rd, sunDir), -1.0, 1.0));
    float radius = root.frame.sun_disc.x, darkening = root.frame.sun_disc.y;
    float edge = max(2.0 * root.frame.right_tan.w / root.frame.options.y, .0004);
    float disc = 1.0 - smoothstep(radius - edge, radius + edge, angle);
    float mu = sqrt(max(1.0 - (angle * angle) / (radius * radius), 0.0));
    float limb = (1.0 - darkening * (1.0 - mu)) / (1.0 - darkening / 3.0);
    color += float3(1.0, .85, .59) * disc * limb * 35.0;
    color += float3(1.0, .57, .25) * .06 * exp(-angle * 34.0);
    return float4(color, 1.0);
}
