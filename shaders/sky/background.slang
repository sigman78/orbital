#include "../scene/frame_bindings.slang"
#include "../scene/view.slang"
#include "../lib/noise.slang"
#include "galaxy_layers.slang"

// The Milky Way for this pixel: the splat sum, drawn at a quarter of the frame
// since it is smooth at a degree and more, read here bilinearly. rgb the
// radiance, w a dust depth: how much the dark splats take from the bright.
float4 milkyWay(float2 pixel)
{
    return textures[TEX_MILKY_WAY].SampleLevel(samplers[SAMPLER_CLAMP], pixel / root.frame.options.xy, 0.0);
}

// The dust's structure below the splats' scale: the dust depth is
// redistributed by three octaves of gradient noise (fBm), the base feature size,
// amplitude, lacunarity and gain from the Sky panel, each octave on its own
// rotated lattice so no grid shows, and each fading out as its features drop
// below a pixel; the mean transmittance is kept.
static const int DUST_OCTAVES = 3;
float3 milkyDust(float3 rd, float depth, float footprint)
{
    float amplitude = root.frame.galaxy.y;
    if (amplitude <= 0.0 || depth <= 0.0) return float3(1.0, 1.0, 1.0);
    float frequency = 1.0 / max(radians(root.frame.galaxy.z), 1e-4); // the base feature size in degrees to lattice cells per radian
    float lacunarity = root.frame.galaxy_more.x, gain = root.frame.galaxy_more.y;
    // Lattices turned against each other: a rotation about (1, 1, 1) by a third of a turn per octave.
    const float3x3 turn = float3x3(.6667, -.3333, .6667, .6667, .6667, -.3333, -.3333, .6667, .6667);
    float clumping = 0.0, weight = 0.0, octaveAmplitude = 1.0;
    float3 p = rd * frequency;
    for (int octave = 0; octave < DUST_OCTAVES; ++octave)
    {
        float visible = saturate(1.0 / (frequency * footprint * 2.0) - 1.0);
        if (visible <= 0.0) break;
        clumping += gradientNoise(p + float(octave) * 7.3) * .5 * octaveAmplitude * visible;
        weight += octaveAmplitude * visible;
        p = mul(turn, p) * lacunarity + 13.1;
        frequency *= lacunarity;
        octaveAmplitude *= gain;
    }
    if (weight <= 0.0) return float3(1.0, 1.0, 1.0);
    float3 reddening = float3(.8, 1.0, 1.35);
    // Extinction redistributed: exp(-tau * (1 + c)) over the mean of exp(-tau) stays near one.
    float3 mean = exp(-depth * reddening * .25), local = exp(-depth * reddening * .25 * (1.0 + 4.0 * amplitude * clumping / weight));
    return local / max(mean, 1e-3);
}

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
    float3 rd = rayDirection(root.frame, position.xy);
    float3 color = float3(.0005, .0007, .0012);
    if (root.frame.stars.w > 0.0)
    {
        if (root.frame.galaxy.w > 1.5)
            color += galaxyOriginal(rd) * root.frame.stars.w;
        else if (root.frame.galaxy.w > .5)
            color += galaxyLayers(rd) * root.frame.stars.w;
        else {
            float4 band = milkyWay(position.xy);
            float3 radiance = band.rgb * root.frame.stars.w;
            if (max(radiance.r, max(radiance.g, radiance.b)) > 2e-4) // the dark sky skips the noise
                radiance *= milkyDust(rd, band.w, length(fwidth(rd)));
            color += radiance;
        }
    }
    else
    {
        // Without the panorama: a band on the real galactic plane (the pole at RA 192.86, Dec +27.13 in the sky frame).
        float galactic = pow(max(0.0, 1.0 - abs(dot(rd, float3(-.868, .456, -.198)))), 24.0);
        color += float3(.008, .009, .014) * galactic * (.4 + fbm(rd * 14.0));
    }
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
