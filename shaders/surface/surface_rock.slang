#include "../scene/frame_bindings.slang"
#include "lighting.slang"
#include "brdf.slang"
#include "../lib/math.slang"
#include "../lib/noise.slang"

// The belt's rocks and the moonlets: three scanned rock sets (albedo,
// normal+height, roughness) mapped triplanar in the rock's object space, so
// there are no seams or pole pinch. Each rock blends a primary and a secondary
// set by seeded macro noise, and close rocks run parallax occlusion on their
// dominant plane. The far tail of the belt draws as billboards, lit once per
// rock in the culling pass, and their splat mask marks their pixels for the
// temporal pass.

static const float ROCK_TILE = 1.4;            // texture repeats per unit of rock radius
static const float ROCK_PARALLAX = .03;        // parallax depth as a fraction of one tile
static const int ROCK_PARALLAX_STEPS = 10;
static const float ROCK_NEAR = .012, ROCK_FAR = .09; // fwidth(object position) per pixel: full detail below, cheap path above
// Descriptor slots of the three sets (albedo, normal+height, roughness);
// selected by branch so every texture access keeps a constant index.
struct RockSample {
    float3 albedo;
    float3 tangentNormal;
    float rough;
};

float4 rockTap(int set, int map, float2 uv, float2 dx, float2 dy)
{
    if (set == 0) return map == 0 ? textures[TEX_ROCK_ALBEDO].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy) : map == 1 ? textures[TEX_ROCK_NORMAL].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy) : textures[TEX_ROCK_ROUGHNESS].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy);
    if (set == 1) return map == 0 ? textures[TEX_ROCK_FACE_ALBEDO].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy) : map == 1 ? textures[TEX_ROCK_FACE_NORMAL].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy) : textures[TEX_ROCK_FACE_ROUGHNESS].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy);
    return map == 0 ? textures[TEX_ROCK_BOULDER_ALBEDO].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy) : map == 1 ? textures[TEX_ROCK_BOULDER_NORMAL].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy) : textures[TEX_ROCK_BOULDER_ROUGHNESS].SampleGrad(samplers[SAMPLER_WRAP_ANISOTROPIC], uv, dx, dy);
}

RockSample sampleRockSet(int set, float2 uv, float2 dx, float2 dy)
{
    RockSample s;
    s.albedo = rockTap(set, 0, uv, dx, dy).rgb;
    s.tangentNormal = rockTap(set, 1, uv, dx, dy).xyz * 2.0 - 1.0;
    s.rough = rockTap(set, 2, uv, dx, dy).r;
    return s;
}

float rockHeight(int set, float2 uv, float2 dx, float2 dy)
{
    return rockTap(set, 1, uv, dx, dy).a;
}

// Parallax occlusion on one plane: marches into the height field along the
// tangent-space view direction and returns the uv where the ray hits.
float2 parallaxUV(int set, float2 uv, float2 dx, float2 dy, float3 viewTS, float depth)
{
    float2 step = -viewTS.xy / max(viewTS.z, .25) * depth / float(ROCK_PARALLAX_STEPS);
    float layer = 0.0, layerStep = 1.0 / float(ROCK_PARALLAX_STEPS);
    float2 previousUV = uv;
    float previousGap = 1.0 - rockHeight(set, uv, dx, dy);
    for (int i = 0; i < ROCK_PARALLAX_STEPS; ++i)
    {
        float2 nextUV = uv + step * float(i + 1);
        float gap = 1.0 - rockHeight(set, nextUV, dx, dy) - (layer + layerStep);
        if (gap <= 0.0)
        {
            // Intersect the segment between the last two samples.
            float t = previousGap / max(previousGap - gap, 1e-4);
            return lerp(previousUV, nextUV, saturate(t));
        }
        previousUV = nextUV;
        previousGap = gap;
        layer += layerStep;
    }
    return previousUV;
}

// Blends a set's three planar taps; normals use the whiteout blend so the
// map's detail is added to the object-space normal on every plane.
void sampleRockTriplanar(int set, float3 p, float3 n, float3 weights, float2 uvShift, float2 dominantOffset,
                         int dominant, out float3 albedo, out float3 normal, out float rough)
{
    float2 uvX = p.zy * ROCK_TILE + uvShift, uvY = p.xz * ROCK_TILE + uvShift * 1.37, uvZ = p.xy * ROCK_TILE + uvShift * .71;
    if (dominant == 0) uvX += dominantOffset; else if (dominant == 1) uvY += dominantOffset; else uvZ += dominantOffset;
    RockSample sX = sampleRockSet(set, uvX, ddx(uvX), ddy(uvX));
    RockSample sY = sampleRockSet(set, uvY, ddx(uvY), ddy(uvY));
    RockSample sZ = sampleRockSet(set, uvZ, ddx(uvZ), ddy(uvZ));
    albedo = sX.albedo * weights.x + sY.albedo * weights.y + sZ.albedo * weights.z;
    rough = sX.rough * weights.x + sY.rough * weights.y + sZ.rough * weights.z;
    // Tangent axes per plane: X plane u = +z, v = +y; Y plane u = +x, v = +z; Z plane u = +x, v = +y.
    float3 nX = float3(abs(sX.tangentNormal.z) * n.x, sX.tangentNormal.y + n.y, sX.tangentNormal.x + n.z);
    float3 nY = float3(sY.tangentNormal.x + n.x, abs(sY.tangentNormal.z) * n.y, sY.tangentNormal.y + n.z);
    float3 nZ = float3(sZ.tangentNormal.xy + n.xy, abs(sZ.tangentNormal.z) * n.z);
    normal = normalize(nX * weights.x + nY * weights.y + nZ * weights.z);
}

Material rockMaterial(SurfaceVaryings input, Instance inst, float3x3 rotate, float3 N, float3 V, float visibility)
{
    float3x3 toObject = transpose(rotate);
    float3 p = mul(toObject, input.worldPosition - inst.center_radius.xyz) / max(inst.center_radius.w, 1e-6);
    float3 n = normalize(mul(toObject, N));
    // Composition class in the integer part, material seed in the fraction (see cull.slang).
    int composition = int(inst.tint.w);
    float seed = frac(inst.tint.w);
    float closeness = 1.0 - smoothstep(ROCK_NEAR, ROCK_FAR, length(fwidth(p)));
    float3 weights = pow(abs(n), 5.0);
    weights /= weights.x + weights.y + weights.z;
    int dominant = weights.x > weights.y ? (weights.x > weights.z ? 0 : 2) : (weights.y > weights.z ? 1 : 2);
    // Silicate rocks use the worn and dusty boulder scans, carbonaceous ones the dark rock face.
    int primary = composition == 1 ? 1 : (seed < .5 ? 0 : 2);
    int secondary = composition == 1 ? (seed < .5 ? 0 : 2) : 1;
    float2 uvShift = float2(seed * 5.3, seed * 2.9);

    // Parallax on the dominant plane, fading in as the rock fills the screen.
    float2 dominantOffset = float2(0.0, 0.0);
    float parallax = smoothstep(.3, 1.0, closeness);
    if (parallax > 0.0)
    {
        float3 v = mul(toObject, V);
        float3 viewTS = dominant == 0 ? float3(v.z, v.y, abs(v.x)) : dominant == 1 ? float3(v.x, v.z, abs(v.y)) : float3(v.x, v.y, abs(v.z));
        float2 uv0 = (dominant == 0 ? p.zy : dominant == 1 ? p.xz : p.xy) * ROCK_TILE + uvShift;
        dominantOffset = parallaxUV(primary, uv0, ddx(uv0), ddy(uv0), viewTS, ROCK_PARALLAX * parallax) - uv0;
    }

    float3 albedoA, normalA, albedoB, normalB;
    float roughA, roughB;
    sampleRockTriplanar(primary, p, n, weights, uvShift, dominantOffset, dominant, albedoA, normalA, roughA);
    // The secondary set shows in seeded patches; far rocks skip it.
    float mixAmount = .2 + .5 * frac(seed * 13.7);
    float patches = smoothstep(.45, .6, fbm(p * 2.5 + seed * 31.0)) * mixAmount;
    if (closeness > 0.05 && patches > 0.01)
        sampleRockTriplanar(secondary, p, n, weights, uvShift + 3.1, dominantOffset, dominant, albedoB, normalB, roughB);
    else { albedoB = albedoA; normalB = normalA; roughB = roughA; patches = 0.0; }

    Material m;
    m.albedo = lerp(albedoA, albedoB, patches) * inst.tint.xyz * (.8 + .4 * frac(seed * 5.1));
    // Scanned microsurface roughness plus stable body-to-body variation.
    // Rock stays dielectric; smoother patches produce broad glints, not metal.
    m.rough = clamp(lerp(roughA, roughB, patches) * .85 + .10 + (frac(seed * 3.3) - .5) * .2, .3, .98);
    float3 objectNormal = normalize(lerp(normalA, normalB, patches));
    // Detail normals fade out with distance so distant rocks do not sparkle.
    objectNormal = normalize(lerp(n, objectNormal, .35 + .65 * closeness));
    m.N = normalize(mul(rotate, objectNormal));
    m.water = 0.0;
    m.visibility = visibility;
    return m;
}

// Splats are lit once per rock in the culling pass (tint.w is the covered
// fraction of the footprint). Discs are sphere impostors: tint holds albedo
// times irradiance and the Lambert cosine is taken per pixel from the sphere
// normal the disc coordinate implies, so a few-pixel rock shows a lit side and
// a dark limb; rotation_kind.y and .z carry the per-rock rim and ambient
// fractions. Points hold their final radiance and only shape the footprint.
float4 shadeBillboard(SurfaceVaryings input, Instance inst)
{
    float2 q = input.localNormal.xy;
    float rr = dot(q, q);
    if (inst.rotation_kind.x > .5)
    {
        float edge = fwidth(rr); // a soft-edged disc of the rock's true size
        float footprint = 1.0 - smoothstep(1.0 - edge, 1.0 + edge, rr);
        float3 n = normalize(root.frame->right_tan.xyz * q.x + root.frame->up_aspect.xyz * q.y
                             - root.frame->forward_exposure.xyz * sqrt(max(0.0, 1.0 - rr)));
        float3 l = normalize(root.frame->sun.xyz - input.worldPosition);
        float shading = max(dot(n, l), 0.0) + inst.rotation_kind.y + inst.rotation_kind.z;
        return float4(inst.tint.xyz * shading, saturate(inst.tint.w * footprint));
    }
    float footprint = exp(-3.0 * rr) / .317; // Gaussian point; integral over the unit disc relative to its area
    return float4(inst.tint.xyz, saturate(inst.tint.w * footprint));
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    if (root.mode == MODE_BILLBOARD) return shadeBillboard(input, root.instances[input.instanceIndex]);
    if (root.mode == MODE_SPLAT_MASK)
    {
        float coverage = shadeBillboard(input, root.instances[input.instanceIndex]).a;
        float z = dot(input.worldPosition, root.frame.forward_exposure.xyz);
        return float4(z * coverage, 0.0, 0.0, coverage);
    }
    Surface s = surfaceAt(root.frame, root.instances, input, true);
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    Material m = rockMaterial(input, s.inst, rotate, s.N, s.V, s.visibility);
    return float4(shadeSurface(m, s.L, s.V, sunlight(root.frame), root.frame->earth.y), 1);
}
