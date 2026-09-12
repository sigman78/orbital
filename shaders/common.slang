#include "scene_shared.h"

[[vk::binding(0, 0)]] Texture2D<float4> textures[32];
[[vk::binding(1, 0)]] SamplerState samplers[4];
[[vk::push_constant]] ConstantBuffer<Root> root;

static const float PI = 3.141592653589793;
static const float NEAR_Z = .02;
static const float FAR_Z = 2000.0;
static const int MAX_BODIES = 8; // Frame.bodies capacity; scene.x holds the live count

float3 rayDirection(float2 pixel)
{
    float2 xy = (pixel - root.frame->jitter.xy) / root.frame->options.xy * 2.0 - 1.0;
    return normalize(root.frame->forward_exposure.xyz + root.frame->right_tan.xyz * xy.x * root.frame->right_tan.w * root.frame->up_aspect.w
                     - root.frame->up_aspect.xyz * xy.y * root.frame->right_tan.w);
}

float2 sphereHit(float3 ro, float3 rd, float3 center, float radius)
{
    float3 oc = ro - center;
    float b = dot(oc, rd), d = b * b - dot(oc, oc) + radius * radius;
    if (d < 0.0) return float2(1e20, -1e20);
    d = sqrt(d);
    return float2(-b - d, -b + d);
}

float linearViewDepth(float d) { return (NEAR_Z * FAR_Z) / max(FAR_Z - d * (FAR_Z - NEAR_Z), 1e-5); }

// Distance along a view ray to the opaque scene at this pixel; very large for sky.
float sceneDistance(float2 pixel, float3 rd)
{
    float d = textures[18].Load(int3(int2(pixel), 0)).r;
    if (d >= .999999) return 1e9;
    return linearViewDepth(d) / max(dot(rd, root.frame->forward_exposure.xyz), 1e-4);
}

float solarVisibility(float3 point, int skip)
{
    float3 light = normalize(root.frame->sun.xyz - point);
    float visibility = 1.0;
    int bodyCount = int(root.frame->scene.x);
    for (int i = 0; i < MAX_BODIES; ++i) if (i < bodyCount && i != skip)
    {
        float3 q = root.frame->bodies[i].xyz - point;
        float t = dot(q, light);
        if (t > 0.0)
        {
            float distanceToRay = length(q - light * t), radius = root.frame->bodies[i].w;
            float penumbra = max(t * 0.007, 0.015);
            visibility *= smoothstep(radius - penumbra, radius + penumbra, distanceToRay);
        }
    }
    return visibility;
}

float localShadow(float3 point, float3 normal)
{
    float4 light = mul(root.frame->light_projection, float4(point + normal * 0.015, 1.0));
    float3 coord = light.xyz / light.w;
    float2 uv = coord.xy * 0.5 + 0.5;
    if (any(uv < 0.001) || any(uv > 0.999) || coord.z < 0.0 || coord.z > 1.0) return 1.0;
    uint width, height;
    textures[15].GetDimensions(width, height);
    float2 texel = 1.0 / float2(width, height);
    float result = 0.0;
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)
        result += textures[15].SampleLevel(samplers[0], uv + float2(x, y) * texel, 0.0).r >= coord.z - 0.0003 ? 1.0 : 0.0;
    return result / 9.0;
}

// Relative rock density across the belt (0 inner edge, 1 outer edge); mirrors
// geometry::belt_ring_density so the shadow shows the same rings and gap.
float beltRingDensity(float t)
{
    float bands = .5 + .5 * cos(2.0 * PI * (t * 3.0 + .15));
    float gap = (t - .58) / .035;
    return (.5 + .5 * bands) * (1.0 - .7 * exp(-gap * gap));
}

// Belt as a translucent slab: relative rock and dust density at a point given
// in the giant's frame (height above the belt plane, distance from the axis).
static const float BELT_EXTINCTION = .4;     // optical depth per unit path at density 1; the far side of a full-thickness path loses about half its light
static const int BELT_EXTINCTION_STEPS = 8;

float beltDensity(float3 q, float4 ring, float3 n)
{
    float h = dot(q, n);
    float r = length(q - n * h);
    float half = ring.w * .5;
    float vertical = 1.0 - smoothstep(half * .6, half * 1.3, abs(h));
    float feather = (ring.y - ring.x) * .08;
    float inside = smoothstep(ring.x - feather, ring.x + feather, r) * (1.0 - smoothstep(ring.y - feather, ring.y + feather, r));
    return vertical * inside * beltRingDensity(saturate((r - ring.x) / (ring.y - ring.x)));
}

// Sunlight left after crossing the belt to reach a point inside it: the
// density is integrated along the sun ray until the ray leaves the slab.
// Rocks on the sunward rim stay bright, rocks buried deep or on the far side
// sit in a smooth dusk. No texels, so nothing shimmers.
float beltExtinction(float3 point)
{
    float4 ring = root.frame->belt_ring;
    if (ring.z <= 0.0 || root.frame->scene.w < .5) return 1.0;
    float3 n = root.frame->belt_normal.xyz;
    float3 q = point - root.frame->bodies[int(root.frame->scene.y)].xyz;
    float3 light = normalize(root.frame->sun.xyz - point);
    // Distance until the ray has left the slab's height band, capped for the near-grazing case.
    float climb = max(abs(dot(light, n)), .08);
    float length = min((ring.w * .5 * 1.3 + abs(dot(q, n))) / climb, ring.y - ring.x);
    float ds = length / float(BELT_EXTINCTION_STEPS);
    float tau = 0.0;
    for (int i = 0; i < BELT_EXTINCTION_STEPS; ++i)
        tau += beltDensity(q + light * ((float(i) + .5) * ds), ring, n) * ds;
    return exp(-tau * ring.z * BELT_EXTINCTION);
}

// Sunlight left after the rocks between a belt point and the sun: the
// transmittance map holds per-texel coverage in four slices along the sun ray
// (see beltsplat.slang); the slices on the sun side of the point attenuate it.
static const float BELT_MAP_OPACITY = 1.0; // optical depth per unit of texel coverage
static const float BELT_MAP_SLICES = 4.0;

float beltRockTransmittance(float3 point)
{
    if (root.frame->scene.z < .5) return 1.0;
    float4 clip = mul(root.frame->belt_light_projection, float4(point, 1.0));
    float2 uv = clip.xy * .5 + .5;
    if (any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    float4 coverage = textures[28].SampleLevel(samplers[0], uv, 0.0);
    float3 n = root.frame->belt_normal.xyz;
    float3 q = point - root.frame->bodies[int(root.frame->scene.y)].xyz;
    float3 light = normalize(root.frame->sun.xyz - point);
    float along = dot(q, n) / (dot(light, n) + 1e-6);
    float c = clamp((along / root.frame->belt_light.w * .5 + .5) * BELT_MAP_SLICES, 0.0, BELT_MAP_SLICES);
    // Only slices wholly on the sun side count: the receiver's own slice holds
    // its own splat, which must not shadow it.
    float sum = 0.0;
    for (int slice = 0; slice < 4; ++slice)
        sum += coverage[slice] * (float(slice) >= c ? 1.0 : 0.0);
    return exp(-sum * BELT_MAP_OPACITY);
}

// Shadow the asteroid belt casts on the gas giant: the sun ray from a surface
// point is intersected with the belt plane and darkened inside the annulus.
// The belt is a sparse cloud, so the shadow is soft and never fully dark.
float beltShadow(float3 point)
{
    float4 ring = root.frame->belt_ring;
    if (ring.z <= 0.0) return 1.0;
    float3 centre = root.frame->bodies[int(root.frame->scene.y)].xyz;
    float3 n = root.frame->belt_normal.xyz;
    float3 light = normalize(root.frame->sun.xyz - point);
    float denominator = dot(light, n);
    if (abs(denominator) < 1e-4) return 1.0;
    float t = dot(centre - point, n) / denominator;
    if (t <= 0.0) return 1.0;
    float3 q = point + light * t - centre;
    float r = length(q);
    float feather = (ring.y - ring.x) * .12;
    float inside = smoothstep(ring.x - feather, ring.x + feather, r) * (1.0 - smoothstep(ring.y - feather, ring.y + feather, r));
    float density = beltRingDensity(saturate((r - ring.x) / (ring.y - ring.x)));
    float grain = .8 + .2 * noise(q * .5);
    return 1.0 - ring.z * .585 * inside * density * grain;
}

float3x3 rotation(float3 a)
{
    float3 c = cos(a), s = sin(a);
    return mul(mul(float3x3(c.z,-s.z,0,s.z,c.z,0,0,0,1), float3x3(c.y,0,s.y,0,1,0,-s.y,0,c.y)),
               float3x3(1,0,0,0,c.x,-s.x,0,s.x,c.x));
}
float2 sphereUV(float3 n) { return float2(atan2(n.z,n.x)/(2.0*PI)+0.5, 0.5-asin(clamp(n.y,-1.0,1.0))/PI); }
float4 sampleSphere(Texture2D<float4> tex, float2 uv)
{
    float2 dx = ddx(uv), dy = ddy(uv);
    dx.x -= round(dx.x); dy.x -= round(dy.x);
    return tex.SampleGrad(samplers[1], uv, dx, dy);
}
float hash(float3 p) { p=frac(p*.1031); p+=dot(p,p.yzx+33.33); return frac((p.x+p.y)*p.z); }
float noise(float3 p)
{
    float3 i=floor(p),f=frac(p); f=f*f*(3.0-2.0*f);
    return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),
                lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);
}
float fbm(float3 p) { float n=0,a=.5; for(int i=0;i<4;i++){n+=a*noise(p);p=p*2.03+float3(2.1,1.7,4.3);a*=.5;} return n; }

// 2D value noise for surface detail; cheaper than the 3D variant above.
float hash2(float2 p) { float3 q = frac(float3(p.xyx) * .1031); q += dot(q, q.yzx + 33.33); return frac((q.x + q.y) * q.z); }
float noise2(float2 p)
{
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(hash2(i), hash2(i + float2(1, 0)), f.x), lerp(hash2(i + float2(0, 1)), hash2(i + float2(1, 1)), f.x), f.y);
}
float fbm2(float2 p, int octaves)
{
    float n = 0.0, a = .5;
    for (int i = 0; i < octaves; ++i) { n += a * noise2(p); p = p * 2.03 + float2(1.7, 9.2); a *= .5; }
    return n;
}

// 1 while a texel of a texture with the given size covers more than a screen
// pixel (magnified), fading to 0 once texels shrink below a pixel. Procedural
// detail is scaled by this so it only appears where the source runs out.
float magnification(float2 uv, float2 texels)
{
    float2 d = max(abs(ddx(uv)), abs(ddy(uv))) * texels;
    float lod = log2(max(max(d.x, d.y), 1e-6));
    return saturate(-lod);
}
