#pragma once
#include "../scene_shared.h"
#include "frame_records.slang"

// Visibility of the solar disc past the scene bodies: those whose centre lies
// between the point and the sun, with a penumbra that widens with distance.
static const int MAX_OCCLUDING_BODIES = 8;
float solarVisibility(Frame* frame, float3 point, int skip)
{
    LightParams sunLight = lightParams(frame);
    float3 toSun = sunLight.position - point;
    float sunDistance = length(toSun);
    float3 light = toSun / max(sunDistance, 1e-6);
    float visibility = 1.0;
    int bodies = bodyCount(frame);
    for (int i = 0; i < MAX_OCCLUDING_BODIES; ++i) if (i < bodies && i != skip)
    {
        BodyGpu body = bodyAt(frame, i);
        float3 q = body.centre - point;
        float t = dot(q, light);
        if (t > 0.0 && t < sunDistance)
        {
            float distanceToRay = length(q - light * t), radius = body.radius;
            float penumbra = max(t * 0.007, 0.015);
            visibility *= smoothstep(radius - penumbra, radius + penumbra, distanceToRay);
        }
    }
    return visibility;
}

// The bodies whose shadow can touch the view segment rd * [t0, t1]: a body's
// shadow is the half-line from its centre away from the sun, widened by the
// radius and the penumbra, and a segment farther from that line than the width
// never enters it. A march takes the mask once and tests only those bodies at
// every sample; most pixels test none.
uint shadowCandidates(Frame* frame, float3 rd, float t0, float t1)
{
    LightParams sunLight = lightParams(frame);
    float3 light = normalize(sunLight.position - rd * (.5 * (t0 + t1)));
    float3 v = -light; // the shadow ray's direction
    uint mask = 0;
    int bodies = bodyCount(frame);
    for (int i = 0; i < MAX_OCCLUDING_BODIES; ++i) if (i < bodies)
    {
        BodyGpu body = bodyAt(frame, i);
        // Closest points of the segment (origin 0, direction rd) and the ray (body.centre, v, s >= 0):
        // the unclamped solution, then the clamped end re-solved against the other line.
        float b = dot(rd, v), d = -dot(rd, body.centre), e = -dot(v, body.centre);
        float denominator = 1.0 - b * b;
        float t = t0, s = 0.0;
        if (denominator > 1e-6)
        {
            t = (b * e - d) / denominator;
            s = (e - b * d) / denominator;
        }
        if (s < 0.0)
        {
            s = 0.0;
            t = clamp(dot(body.centre, rd), t0, t1);
        }
        else
        {
            t = clamp(t, t0, t1);
            s = max(dot(rd * t - body.centre, v), 0.0);
        }
        float distance = length(rd * t - (body.centre + v * s));
        float width = body.radius + max(s * 0.007, 0.015) + 0.05;
        if (distance < width) mask |= 1u << i;
    }
    return mask;
}

// solarVisibility over the bodies in the mask only.
float solarVisibilityMasked(Frame* frame, float3 point, uint mask)
{
    if (mask == 0) return 1.0;
    LightParams sunLight = lightParams(frame);
    float3 toSun = sunLight.position - point;
    float sunDistance = length(toSun);
    float3 light = toSun / max(sunDistance, 1e-6);
    float visibility = 1.0;
    for (int i = 0; i < MAX_OCCLUDING_BODIES; ++i) if ((mask >> i) & 1u)
    {
        BodyGpu body = bodyAt(frame, i);
        float3 q = body.centre - point;
        float t = dot(q, light);
        if (t > 0.0 && t < sunDistance)
        {
            float distanceToRay = length(q - light * t), radius = body.radius;
            float penumbra = max(t * 0.007, 0.015);
            visibility *= smoothstep(radius - penumbra, radius + penumbra, distanceToRay);
        }
    }
    return visibility;
}
