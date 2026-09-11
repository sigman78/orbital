#include "scene_shared.h"

[[vk::binding(0, 0)]] Texture2D<float4> textures[24];
[[vk::binding(1, 0)]] SamplerState samplers[4];
[[vk::push_constant]] ConstantBuffer<Root> root;

static const float PI = 3.141592653589793;

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

float solarVisibility(float3 point, int skip)
{
    float3 light = normalize(root.frame->sun.xyz - point);
    float visibility = 1.0;
    for (int i = 0; i < 3; ++i) if (i != skip)
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
