#pragma once
#include "../scene_shared.h"

// Visibility of the solar disc past the scene bodies.
static const int MAX_OCCLUDING_BODIES = 8;
float solarVisibility(Frame* frame, float3 point, int skip)
{
    float3 light = normalize(frame->sun.xyz - point);
    float visibility = 1.0;
    int bodyCount = int(frame->scene.x);
    for (int i = 0; i < MAX_OCCLUDING_BODIES; ++i) if (i < bodyCount && i != skip)
    {
        float3 q = frame->bodies[i].xyz - point;
        float t = dot(q, light);
        if (t > 0.0)
        {
            float distanceToRay = length(q - light * t), radius = frame->bodies[i].w;
            float penumbra = max(t * 0.007, 0.015);
            visibility *= smoothstep(radius - penumbra, radius + penumbra, distanceToRay);
        }
    }
    return visibility;
}
