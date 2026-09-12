#include "common.slang"

// Root.mode values (SurfaceMode on the C++ side).
static const uint MODE_OPAQUE = 0, MODE_CLOUD = 1, MODE_SHADOW = 2, MODE_BILLBOARD = 3;
// Instance kinds (rotation_kind.w); SurfaceKind on the C++ side.
static const int KIND_EARTH = 0, KIND_GIANT = 1, KIND_MOON = 2, KIND_ROCK = 3, KIND_MARS = 4;

static const float CLOUD_DRIFT = .00012;      // uv per second; Earth itself turns 1/1600 of a revolution per second
static const float CLOUD_HEIGHT = .009;       // cloud shell above the surface, in radii (matches the 1.009 mesh scale)
static const float EARTH_NORMAL_Y = 1.0;      // the sourced map is DirectX-style (green = south), matching B below
static const float2 EARTH_TEXELS = float2(4096.0, 2048.0);
static const float2 GAS_TEXELS = float2(2048.0, 1024.0);
static const float2 RELIEF_TEXELS = float2(4096.0, 2048.0); // Moon and Mars albedo and normal+height maps
// Height range of the baked DEM alpha channel in radii (manifest height_min_m..height_max_m over the body radius).
static const float MOON_HEIGHT_RANGE = 19603.0 / 1737400.0;
static const float MARS_HEIGHT_RANGE = 29334.0 / 3396200.0;

struct SurfaceVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPosition : WORLD_POSITION;
    [[vk::location(1)]] float3 worldNormal : WORLD_NORMAL;
    [[vk::location(2)]] float3 localNormal : LOCAL_NORMAL;
    [[vk::location(3)]] nointerpolation uint instanceIndex : INSTANCE_INDEX;
};

[shader("vertex")]
SurfaceVaryings vertexMain(uint vertexIndex : SV_VertexID, uint firstVertex : SV_StartVertexLocation,
                           uint instanceId : SV_InstanceID, uint firstInstance : SV_StartInstanceLocation)
{
    // SV_VertexID and SV_InstanceID are zero based per draw; the pooled rock
    // draws place their group's slice through the vertex-offset and
    // first-instance arguments.
    vertexIndex += firstVertex;
    SurfaceVaryings output;
    output.instanceIndex = root.base + firstInstance + instanceId;
    Instance inst = root.instances[output.instanceIndex];
    if (root.mode == MODE_BILLBOARD)
    {
        const float2 corners[6] = {float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
        float2 q = corners[vertexIndex];
        output.worldPosition = inst.center_radius.xyz + (root.frame->right_tan.xyz*q.x + root.frame->up_aspect.xyz*q.y)*inst.center_radius.w;
        output.worldNormal = -root.frame->forward_exposure.xyz; output.localNormal = float3(q,0);
        output.position = mul(root.frame->view_projection, float4(output.worldPosition,1));
        return output;
    }
    Vertex vertex = root.vertices[vertexIndex];
    float3x3 rotate = rotation(inst.rotation_kind.xyz);
    float scale = inst.center_radius.w * (root.mode == MODE_CLOUD ? 1.0 + CLOUD_HEIGHT : 1.0);
    output.worldPosition = inst.center_radius.xyz + mul(rotate, vertex.position.xyz) * scale;
    output.worldNormal = normalize(mul(rotate, vertex.normal.xyz)); output.localNormal = normalize(vertex.position.xyz);
    output.position = mul(root.mode == MODE_SHADOW ? root.frame->light_projection : root.frame->view_projection, float4(output.worldPosition, 1.0));
    return output;
}

// Tangent frame of an equirectangular map at a local normal: T points east
// (+u), B points south (+v), both rotated into world space.
void tangentFrame(float3 localN, float3x3 rotate, float3 N, out float3 T, out float3 B)
{
    float3 localT = float3(-localN.z, 0, localN.x);
    if (dot(localT, localT) < 1e-8) localT = float3(1, 0, 0);
    T = normalize(mul(rotate, normalize(localT)));
    T = normalize(T - N * dot(N, T));
    B = normalize(cross(N, T));
}

// --- Earth clouds ---------------------------------------------------------------

// Cloud coordinate: a slow drift plus a slower, larger swirl that follows it.
float2 cloudUV(float2 uv, float t)
{
    uv.x += t * CLOUD_DRIFT;
    float2 swirl = float2(fbm2(uv * float2(6, 12) + t * .003, 3),
                          fbm2(uv * float2(6, 12) + float2(11.3, 4.7) - t * .002, 3)) - .5;
    return uv + swirl * float2(.006, .004);
}

// Cloud coverage with fine structure added where the 4K mask is magnified.
float cloudDensity(float2 uv, float t, float detail)
{
    float density = sampleSphere(textures[5], uv).a;
    density *= 1.0 + (fbm2(uv * float2(1024, 512) + t * .01, 3) - .5) * .6 * detail;
    return pow(saturate(density), 1.3);
}

float4 shadeClouds(SurfaceVaryings input, float3 N, float3 L, float visibility, float nl)
{
    float t = root.frame->camera_time.w;
    float2 uv = sphereUV(normalize(input.localNormal));
    float alpha = cloudDensity(cloudUV(uv, t), t, magnification(uv, EARTH_TEXELS)) * .88;
    float3 cloudLight = float3(.002, .004, .008) + float3(1.0, .99, .97) * pow(nl, .8) * visibility * root.frame->sun.w / PI * .8;
    return float4(cloudLight, alpha);
}

// --- Airless relief (Moon, Mars) ------------------------------------------------

// Short shadow trace through the DEM height stored in the normal map's alpha:
// marches up-sun in texture space and darkens where terrain rises above the
// light ray. heightRange converts alpha to radii.
float reliefShadow(Texture2D<float4> normalMap, float2 uv, float3 L, float3 N, float3 T, float3 B, float heightRange)
{
    float2 lightUV = float2(dot(L, T), dot(L, B));
    float lightRise = max(dot(L, N), .02) / max(length(lightUV), 1e-4); // rise per unit of tangent distance
    lightUV = normalize(lightUV + 1e-6);
    float2 texel = 1.0 / RELIEF_TEXELS;
    float arc = 2.0 * PI / RELIEF_TEXELS.x; // tangent distance of one texel, in radii
    float h0 = normalMap.SampleLevel(samplers[1], uv, 1.0).a * heightRange;
    float shadow = 0.0;
    for (int s = 1; s <= 6; ++s)
    {
        float distance = float(s) * 1.5;
        float h = normalMap.SampleLevel(samplers[1], uv + lightUV * texel * distance, 1.0).a * heightRange;
        float rise = h - h0 - lightRise * distance * arc;
        shadow = max(shadow, rise / (distance * arc) * 6.0);
    }
    return 1.0 - saturate(shadow) * .55;
}

Material reliefMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility,
                        Texture2D<float4> albedoMap, Texture2D<float4> normalMap, float heightRange, float strength)
{
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float detail = magnification(uv, RELIEF_TEXELS);
    float3 T, B;
    tangentFrame(localN, rotate, N, T, B);
    float3 mapN = sampleSphere(normalMap, uv).xyz * 2.0 - 1.0;
    Material m;
    m.N = normalize(T * mapN.x * strength + B * mapN.y * strength + N * max(mapN.z, .2));
    m.visibility = visibility * reliefShadow(normalMap, uv, L, N, T, B, heightRange);
    m.albedo = sampleSphere(albedoMap, uv).rgb;
    // Regolith grain where the 4K maps are magnified.
    m.albedo *= 1.0 + (fbm2(uv * float2(3072, 1536), 3) - .5) * .10 * detail;
    m.rough = .95;
    m.water = 0.0;
    return m;
}

// --- Fragment ---------------------------------------------------------------------

struct Material {
    float3 albedo;
    float3 N;      // shading normal
    float rough;
    float water;   // 0 land, 1 ocean
    float visibility;
};

Material earthMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility)
{
    float t = root.frame->camera_time.w;
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float detail = magnification(uv, EARTH_TEXELS);
    Material m;
    m.albedo = sampleSphere(textures[3], uv).rgb;
    m.water = sampleSphere(textures[8], uv).r;
    // Land grain where the 4K map is magnified; the sea stays smooth.
    m.albedo *= 1.0 + (fbm2(uv * float2(8192, 4096), 3) - .5) * .3 * detail * (1.0 - m.water * .7);
    m.rough = lerp(.85, .085, m.water);
    float3 T, B;
    tangentFrame(localN, rotate, N, T, B);
    float3 mapN = sampleSphere(textures[7], uv).xyz * 2.0 - 1.0;
    mapN.y *= EARTH_NORMAL_Y;
    m.N = normalize(T * mapN.x * .55 + B * mapN.y * .55 + N * max(mapN.z, .2));
    // Cloud shadow: the cloud above this point along the sun direction.
    float3 localL = mul(transpose(rotate), L);
    float3 towardSun = normalize(localN + localL * (CLOUD_HEIGHT / max(dot(localN, localL), .25)));
    float shadowCloud = cloudDensity(cloudUV(sphereUV(towardSun), t), t, detail);
    m.visibility = visibility * (1.0 - shadowCloud * .6);
    return m;
}

Material giantMaterial(SurfaceVaryings input, float3 N, float visibility)
{
    float t = root.frame->camera_time.w;
    float2 uv = sphereUV(normalize(input.localNormal));
    float lat = uv.y * 2.0 - 1.0;
    // Zonal jets: bands drift east or west by latitude, slowly.
    float wind = (sin(lat * PI * 5.0) * .5 + cos(lat * PI * 11.0) * .25) * .00035;
    float2 flow = float2(uv.x + t * wind, uv.y);
    // Large, slow vortices: a domain warp that turns over minutes.
    float2 warp = float2(fbm2(flow * float2(10, 20) + t * .0015, 3),
                         fbm2(flow * float2(10, 20) + float2(5.2, 1.3) - t * .001, 3)) - .5;
    float2 uvA = flow + warp * float2(.010, .005);
    float2 uvB = flow + warp.yx * float2(-.006, .004) + float2(.003, 0);
    float3 lower = sampleSphere(textures[4], uvA).rgb;
    float3 upper = sampleSphere(textures[4], uvB).rgb * float3(1.03, .99, .92);
    float layerMask = smoothstep(.35, .65, fbm2(flow * float2(4, 9) + t * .0008, 3));
    Material m;
    m.albedo = lerp(lower, upper, layerMask * .6);
    // Storm systems as broad brightness variation, and fine turbulence once
    // the 2K map is magnified so its softness never shows.
    m.albedo *= 1.0 + (fbm2(flow * float2(60, 120) - t * .002, 2) - .5) * .12;
    float fine = magnification(uv, GAS_TEXELS);
    float2 streamUV = uvA + warp.yx * float2(.02, .004);
    m.albedo *= 1.0 + (fbm2(streamUV * float2(700, 350) + t * .004, 3) - .5) * .30 * fine;
    m.albedo *= 1.0 + (fbm2(streamUV * float2(2400, 480) - t * .003, 2) - .5) * .18 * fine;
    m.N = N;
    m.rough = .95;
    m.water = 0.0;
    m.visibility = visibility * beltShadow(input.worldPosition);
    return m;
}

Material moonMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility)
{
    return reliefMaterial(input, rotate, N, L, visibility, textures[10], textures[21], MOON_HEIGHT_RANGE, 1.0);
}

Material marsMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility)
{
    // MOLA slopes are gentle at 5 km per texel; doubled so the relief reads.
    Material m = reliefMaterial(input, rotate, N, L, visibility, textures[19], textures[20], MARS_HEIGHT_RANGE, 2.0);
    m.rough = .9;
    return m;
}

// --- Rocks ----------------------------------------------------------------------
//
// Three scanned rock sets (albedo, normal+height, roughness) are mapped
// triplanar in the rock's object space, so there are no seams or pole pinch.
// Each rock blends a primary and a secondary set by seeded macro noise, and
// close rocks run parallax occlusion on their dominant plane.

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
    if (set == 0) return map == 0 ? textures[11].SampleGrad(samplers[3], uv, dx, dy) : map == 1 ? textures[12].SampleGrad(samplers[3], uv, dx, dy) : textures[13].SampleGrad(samplers[3], uv, dx, dy);
    if (set == 1) return map == 0 ? textures[22].SampleGrad(samplers[3], uv, dx, dy) : map == 1 ? textures[23].SampleGrad(samplers[3], uv, dx, dy) : textures[24].SampleGrad(samplers[3], uv, dx, dy);
    return map == 0 ? textures[25].SampleGrad(samplers[3], uv, dx, dy) : map == 1 ? textures[26].SampleGrad(samplers[3], uv, dx, dy) : textures[27].SampleGrad(samplers[3], uv, dx, dy);
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
    float seed = inst.tint.w;
    float closeness = 1.0 - smoothstep(ROCK_NEAR, ROCK_FAR, length(fwidth(p)));
    float3 weights = pow(abs(n), 5.0);
    weights /= weights.x + weights.y + weights.z;
    int dominant = weights.x > weights.y ? (weights.x > weights.z ? 0 : 2) : (weights.y > weights.z ? 1 : 2);
    int primary = int(seed * 3.0) % 3, secondary = (primary + 1 + int(seed * 7.0) % 2) % 3;
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

float4 shadeBillboard(SurfaceVaryings input, Instance inst)
{
    float2 q = input.localNormal.xy;
    float rr = dot(q, q);
    float coverage = 1.0 - smoothstep(1.0 - fwidth(rr), 1.0 + fwidth(rr), rr);
    float3 n = normalize(root.frame->right_tan.xyz * q.x + root.frame->up_aspect.xyz * q.y - root.frame->forward_exposure.xyz * sqrt(max(0, 1 - rr)));
    float3 l = normalize(root.frame->sun.xyz - input.worldPosition);
    float3 averageRock = textures[11].SampleLevel(samplers[1], float2(.5), 10).rgb;
    float3 radiance = averageRock * inst.tint.xyz * root.frame->sun.w / PI * max(dot(n, l), .025)
                    * solarVisibility(input.worldPosition, -1) * beltExtinction(input.worldPosition)
                    * beltRockTransmittance(input.worldPosition);
    return float4(radiance, coverage * inst.tint.w);
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Instance inst = root.instances[input.instanceIndex];
    int kind = int(inst.rotation_kind.w + .5);
    if (root.mode == MODE_BILLBOARD) return shadeBillboard(input, inst);

    float3 N = normalize(input.worldNormal);
    float3 V = normalize(root.frame->camera_time.xyz - input.worldPosition);
    float3 L = normalize(root.frame->sun.xyz - input.worldPosition);
    int selfBody = input.instanceIndex < uint(root.frame->scene.x) ? int(input.instanceIndex) : -1;
    // Belt rocks lose light to the belt itself; bodies use the planet-anchored shadow map.
    float shadow = selfBody < 0 ? beltExtinction(input.worldPosition) * beltRockTransmittance(input.worldPosition)
                                : localShadow(input.worldPosition, N);
    float visibility = solarVisibility(input.worldPosition, selfBody) * shadow;
    float nl = max(dot(N, L), 0.0);
    if (root.mode == MODE_CLOUD) return shadeClouds(input, N, L, visibility, nl);

    float3x3 rotate = rotation(inst.rotation_kind.xyz);
    Material m;
    if (kind == KIND_EARTH) m = earthMaterial(input, rotate, N, L, visibility);
    else if (kind == KIND_GIANT) m = giantMaterial(input, N, visibility);
    else if (kind == KIND_MOON) m = moonMaterial(input, rotate, N, L, visibility);
    else if (kind == KIND_MARS) m = marsMaterial(input, rotate, N, L, visibility);
    else m = rockMaterial(input, inst, rotate, N, V, visibility);

    // Normal variance broadens unresolved highlights instead of sparkling.
    float3 normalDx = ddx(m.N), normalDy = ddy(m.N);
    float normalVariance = min(.20, .25 * max(dot(normalDx, normalDx), dot(normalDy, normalDy)));
    float rough = sqrt(min(1.0, m.rough * m.rough + normalVariance));
    N = m.N;
    nl = max(dot(N, L), 0.0);
    float3 H = normalize(L + V);
    float nh = max(dot(N, H), 0.0), nv = max(dot(N, V), .001), vh = max(dot(V, H), 0.0);
    float a = rough * rough, a2 = a * a, den = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / max(PI * den * den, 1e-5), k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float G = (nl / (nl * (1 - k) + k)) * (nv / (nv * (1 - k) + k));
    float3 F0 = float3(lerp(.04, .02, m.water));
    float3 F = F0 + (1 - F0) * pow(1 - vh, 5);
    float3 spec = F * D * G / max(4 * nv * nl, .001);
    float3 light = float3(1.0, .99, .97) * root.frame->sun.w;
    float3 diffuse = m.albedo * lerp(1.0, .22, m.water) * (1 - F0) / PI;
    float3 color = (diffuse + spec) * light * nl * m.visibility + m.albedo * float3(.003, .004, .008);
    if (kind == KIND_EARTH)
    {
        float2 uv = sphereUV(normalize(input.localNormal));
        color += sampleSphere(textures[9], uv).rgb * pow(1 - smoothstep(-.15, .1, dot(N, L)), 2.0) * .65;
    }
    return float4(color, 1);
}
