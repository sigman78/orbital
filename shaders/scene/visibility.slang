#pragma once
#include "../scene_shared.h"

// Visibility of the solar disc past the scene bodies: those whose centre lies
// between the point and the sun, with a penumbra that widens with distance.
static const int MAX_OCCLUDING_BODIES = 8;
float solarVisibility(Frame* frame, float3 point, int skip)
{
    float3 toSun = frame->sun.xyz - point;
    float sunDistance = length(toSun);
    float3 light = toSun / max(sunDistance, 1e-6);
    float visibility = 1.0;
    int bodyCount = int(frame->scene.x);
    for (int i = 0; i < MAX_OCCLUDING_BODIES; ++i) if (i < bodyCount && i != skip)
    {
        float3 q = frame->bodies[i].xyz - point;
        float t = dot(q, light);
        if (t > 0.0 && t < sunDistance)
        {
            float distanceToRay = length(q - light * t), radius = frame->bodies[i].w;
            float penumbra = max(t * 0.007, 0.015);
            visibility *= smoothstep(radius - penumbra, radius + penumbra, distanceToRay);
        }
    }
    return visibility;
}
