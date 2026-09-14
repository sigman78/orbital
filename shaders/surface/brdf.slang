#pragma once
#include "types.slang"
#include "../lib/math.slang"

// Dielectric GGX plus Lambert diffuse, with explicit illumination and water-glint inputs.
// `fill` scales the starlight fill on shadow sides (1 the former constant); it
// was a blue veil over the Earth's night side, so the panel holds it low.
float3 shadeSurface(Material m, float3 L, float3 V, float3 irradiance, float glintIntensity, float fill)
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
    spec *= lerp(1.0, glintIntensity, m.water); // the panel's glint intensity, sea only
    float3 diffuse = m.albedo * lerp(1.0, .22, m.water) * (1 - F0) / PI;
    return (diffuse + spec) * irradiance * nl * m.visibility + m.albedo * float3(.003, .004, .008) * fill;
}
