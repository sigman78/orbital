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
