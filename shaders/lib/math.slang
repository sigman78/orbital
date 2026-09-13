#pragma once
// Pure geometry helpers. Directions passed to sphereHit must be normalized.
static const float PI = 3.141592653589793;

float2 sphereHit(float3 ro, float3 rd, float3 center, float radius)
{
    float3 oc = ro - center;
    float b = dot(oc, rd), d = b * b - dot(oc, oc) + radius * radius;
    if (d < 0.0) return float2(1e20, -1e20);
    d = sqrt(d);
    return float2(-b - d, -b + d);
}

float3x3 rotation(float3 a)
{
    float3 c = cos(a), s = sin(a);
    return mul(mul(float3x3(c.z,-s.z,0,s.z,c.z,0,0,0,1), float3x3(c.y,0,s.y,0,1,0,-s.y,0,c.y)),
               float3x3(1,0,0,0,c.x,-s.x,0,s.x,c.x));
}

float2 sphereUV(float3 n) { return float2(atan2(n.z,n.x)/(2.0*PI)+0.5, 0.5-asin(clamp(n.y,-1.0,1.0))/PI); }

void tangentFrame(float3 localN, float3x3 rotate, float3 N, out float3 T, out float3 B)
{
    float3 localT = float3(-localN.z, 0, localN.x);
    if (dot(localT, localT) < 1e-8) localT = float3(1, 0, 0);
    T = normalize(mul(rotate, normalize(localT)));
    T = normalize(T - N * dot(N, T));
    B = normalize(cross(N, T));
}
