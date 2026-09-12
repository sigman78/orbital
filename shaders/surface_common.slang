// Shared by the surface shaders: the vertex stage's varyings, the mode and
// kind numbers, the tangent frame of an equirectangular map, the material a
// body's shader produces, the sun visibility every body starts from and the
// dielectric BRDF the solid bodies share. Each body kind has its own fragment
// shader (surface_earth, surface_giant, surface_airless, surface_rock), so a
// GPU compiles and allocates registers for one material at a time.
#include "common.slang"
#include "clouds.slang"

// Root.mode values (SurfaceMode on the C++ side).
static const uint MODE_OPAQUE = ORBITAL_SURFACE_OPAQUE, MODE_CLOUD = ORBITAL_SURFACE_CLOUD,
                  MODE_SHADOW = ORBITAL_SURFACE_SHADOW, MODE_BILLBOARD = ORBITAL_SURFACE_BILLBOARD,
                  MODE_SPLAT_MASK = ORBITAL_SURFACE_SPLAT_MASK;
// Instance kinds (rotation_kind.w); SurfaceKind on the C++ side.
static const int KIND_EARTH = ORBITAL_KIND_EARTH, KIND_GIANT = ORBITAL_KIND_GIANT, KIND_MOON = ORBITAL_KIND_MOON,
                 KIND_ROCK = ORBITAL_KIND_ROCK, KIND_MARS = ORBITAL_KIND_MARS;

struct SurfaceVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPosition : WORLD_POSITION;
    [[vk::location(1)]] float3 worldNormal : WORLD_NORMAL;
    [[vk::location(2)]] float3 localNormal : LOCAL_NORMAL;
    [[vk::location(3)]] nointerpolation uint instanceIndex : INSTANCE_INDEX;
};

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

struct Material {
    float3 albedo;
    float3 N;      // shading normal
    float rough;
    float water;   // 0 land, 1 ocean
    float visibility;
};

// What every body's fragment starts from: the instance, the geometric normal,
// the view and light directions and the sun's visibility at the point.
struct Surface {
    Instance inst;
    int kind;
    float3 N, V, L;
    float visibility;
};

// Belt rocks lose light to the belt itself; bodies use the planet-anchored
// shadow map when asked to. The gas giant does not: its shadows are all
// analytic (the bodies' and the belt's) and its lighting reaches a few degrees
// past the geometric terminator, where a depth map self-shadows a sphere in
// texel steps.
Surface surfaceAt(SurfaceVaryings input, bool shadowMap)
{
    Surface s;
    s.inst = root.instances[input.instanceIndex];
    s.kind = int(s.inst.rotation_kind.w + .5);
    s.N = normalize(input.worldNormal);
    s.V = normalize(root.frame->camera_time.xyz - input.worldPosition);
    s.L = normalize(root.frame->sun.xyz - input.worldPosition);
    int selfBody = input.instanceIndex < uint(root.frame->scene.x) ? int(input.instanceIndex) : -1;
    float shadow = selfBody < 0 ? beltExtinction(input.worldPosition) * beltRockTransmittance(input.worldPosition)
                                : (shadowMap ? localShadow(input.worldPosition, s.N) : 1.0);
    s.visibility = solarVisibility(input.worldPosition, selfBody) * shadow;
    return s;
}

float3 sunlight() { return float3(1.0, .99, .97) * root.frame->sun.w; }

// The solid bodies' dielectric BRDF: Lambert diffuse and GGX specular, with
// the sea's glint intensity from the panel where the material is water.
float3 shadeSurface(Material m, float3 L, float3 V)
{
    // Normal variance broadens unresolved highlights instead of sparkling.
    float3 normalDx = ddx(m.N), normalDy = ddy(m.N);
    float normalVariance = min(.20, .25 * max(dot(normalDx, normalDx), dot(normalDy, normalDy)));
    float rough = sqrt(min(1.0, m.rough * m.rough + normalVariance));
    float3 N = m.N;
    float nl = max(dot(N, L), 0.0);
    float3 H = normalize(L + V);
    float nh = max(dot(N, H), 0.0), nv = max(dot(N, V), .001), vh = max(dot(V, H), 0.0);
    float a = rough * rough, a2 = a * a, den = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / max(PI * den * den, 1e-5), k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float G = (nl / (nl * (1 - k) + k)) * (nv / (nv * (1 - k) + k));
    float3 F0 = float3(lerp(.04, .02, m.water));
    float3 F = F0 + (1 - F0) * pow(1 - vh, 5);
    float3 spec = F * D * G / max(4 * nv * nl, .001);
    spec *= lerp(1.0, root.frame->earth.y, m.water); // the panel's glint intensity, sea only
    float3 diffuse = m.albedo * lerp(1.0, .22, m.water) * (1 - F0) / PI;
    return (diffuse + spec) * sunlight() * nl * m.visibility + m.albedo * float3(.003, .004, .008);
}
