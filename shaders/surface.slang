#include "common.slang"

// Root.mode values (SurfaceMode on the C++ side).
static const uint MODE_OPAQUE = 0, MODE_CLOUD = 1, MODE_SHADOW = 2, MODE_BILLBOARD = 3;
// Instance kinds (rotation_kind.w).
static const int KIND_EARTH = 0, KIND_GIANT = 1, KIND_MOON = 2, KIND_ROCK = 3;

static const float CLOUD_DRIFT = .00012;      // uv per second; Earth itself turns 1/1600 of a revolution per second
static const float CLOUD_HEIGHT = .009;       // cloud shell above the surface, in radii (matches the 1.009 mesh scale)
static const float EARTH_NORMAL_Y = 1.0;      // the sourced map is DirectX-style (green = south), matching B below
static const float MOON_RELIEF = 1.8;         // relief texels of height per unit of albedo luminance
static const float MOON_RELIEF_LOD = 1.0;     // mip the height field is read from; higher is smoother
static const float2 EARTH_TEXELS = float2(4096.0, 2048.0);
static const float2 GAS_TEXELS = float2(2048.0, 1024.0);
static const float2 MOON_TEXELS = float2(2048.0, 1024.0);

struct SurfaceVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPosition : WORLD_POSITION;
    [[vk::location(1)]] float3 worldNormal : WORLD_NORMAL;
    [[vk::location(2)]] float3 localNormal : LOCAL_NORMAL;
    [[vk::location(3)]] nointerpolation uint instanceIndex : INSTANCE_INDEX;
};

[shader("vertex")]
SurfaceVaryings vertexMain(uint vertexIndex : SV_VertexID, uint instanceId : SV_InstanceID)
{
    SurfaceVaryings output;
    output.instanceIndex = root.base + instanceId;
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

// --- Moon relief ------------------------------------------------------------------

// Height field for the moon: the sourced albedo reads as relief (bright rims,
// dark floors and low maria). It is read from a softer mip so texel grain does
// not turn into pebbles, with broad procedural undulation on top.
float moonHeight(float2 uv)
{
    float2 reliefTexels = MOON_TEXELS / exp2(MOON_RELIEF_LOD);
    const float3 grey = float3(.2126, .7152, .0722);
    float fine = dot(textures[10].SampleLevel(samplers[1], smoothTexel(uv, reliefTexels), MOON_RELIEF_LOD).rgb, grey);
    float broad = dot(textures[10].SampleLevel(samplers[1], uv, MOON_RELIEF_LOD + 2.5).rgb, grey);
    // Broad brightness is terrain (dark maria lie low); small bright features are
    // fresh craters and their ejecta, so those are sunk rather than raised.
    return broad * .6 - (fine - broad) * .8 + (fbm2(uv * float2(160, 80), 4) - .5) * .12;
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
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float detail = magnification(uv, MOON_TEXELS);
    float2 texel = exp2(MOON_RELIEF_LOD) / MOON_TEXELS; // a texel of the relief mip
    float3 T, B;
    tangentFrame(localN, rotate, N, T, B);
    // Relief normal from the height field; slopes are in relief-texel units.
    float h0 = moonHeight(uv);
    float hx = moonHeight(uv + float2(texel.x, 0)) - moonHeight(uv - float2(texel.x, 0));
    float hy = moonHeight(uv + float2(0, texel.y)) - moonHeight(uv - float2(0, texel.y));
    float3 mapN = normalize(float3(-hx * MOON_RELIEF * .5, -hy * MOON_RELIEF * .5, 1.0));
    Material m;
    m.N = normalize(T * mapN.x + B * mapN.y + N * mapN.z);
    // Self shadowing: march up-sun in texture space and compare terrain height
    // with the light ray's height above the surface.
    float2 lightUV = float2(dot(L, T), dot(L, B));
    float lightRise = max(dot(L, N), .02) / max(length(lightUV), 1e-4);
    lightUV = normalize(lightUV + 1e-6);
    float shadow = 0.0;
    for (int s = 1; s <= 4; ++s)
    {
        float distance = float(s) * 1.25;
        float rise = (moonHeight(uv + lightUV * texel * distance) - h0) * MOON_RELIEF;
        shadow = max(shadow, rise - lightRise * distance);
    }
    m.visibility = visibility * (1.0 - saturate(shadow) * .35);
    m.albedo = sampleSphere(textures[10], uv).rgb * .8;
    m.albedo *= 1.0 + (fbm2(uv * float2(1536, 768), 3) - .5) * .08 * detail;
    m.rough = .95;
    m.water = 0.0;
    return m;
}

Material rockMaterial(SurfaceVaryings input, Instance inst, float3 N, float visibility)
{
    float2 uv = sphereUV(normalize(input.localNormal));
    float2 rockUV = uv * 3.0 + inst.tint.x, dx = ddx(uv), dy = ddy(uv);
    dx.x -= round(dx.x); dy.x -= round(dy.x); dx *= 3; dy *= 3;
    Material m;
    m.albedo = textures[11].SampleGrad(samplers[3], rockUV, dx, dy).rgb * inst.tint.xyz;
    // Scanned microsurface roughness plus stable body-to-body variation.
    // Rock stays dielectric; smoother patches produce broad glints, not metal.
    m.rough = clamp(textures[13].SampleGrad(samplers[3], rockUV, dx, dy).r * .85 + .10 + (inst.tint.x - .95) * .5, .24, .98);
    float3 mapN = textures[12].SampleGrad(samplers[3], rockUV, dx, dy).xyz * 2.0 - 1.0;
    float3 tangent = normalize(cross(N, abs(N.y) < .9 ? float3(0, 1, 0) : float3(1, 0, 0)));
    m.N = normalize(tangent * mapN.x * .28 + cross(N, tangent) * mapN.y * .28 + N * max(mapN.z, .3));
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
    float3 radiance = averageRock * inst.tint.xyz * root.frame->sun.w / PI * max(dot(n, l), .025) * solarVisibility(input.worldPosition, -1);
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
    float visibility = solarVisibility(input.worldPosition, kind < KIND_ROCK ? kind : -1) * localShadow(input.worldPosition, N);
    float nl = max(dot(N, L), 0.0);
    if (root.mode == MODE_CLOUD) return shadeClouds(input, N, L, visibility, nl);

    float3x3 rotate = rotation(inst.rotation_kind.xyz);
    Material m;
    if (kind == KIND_EARTH) m = earthMaterial(input, rotate, N, L, visibility);
    else if (kind == KIND_GIANT) m = giantMaterial(input, N, visibility);
    else if (kind == KIND_MOON) m = moonMaterial(input, rotate, N, L, visibility);
    else m = rockMaterial(input, inst, N, visibility);

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
