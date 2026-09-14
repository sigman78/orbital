#pragma once
#include "types.slang"
#include "../scene/instances.slang"
#include "../scene/shadows.slang"

Surface surfaceAt(Frame* frame, Instance* instances, SurfaceVaryings input, bool shadowMap)
{
    Surface s;
    s.inst = loadInstance(frame, instances, input.instanceIndex, false);
    s.kind = int(s.inst.rotation_kind.w + .5);
    s.N = normalize(input.worldNormal);
    s.V = normalize(frame->camera_time.xyz - input.worldPosition);
    s.L = normalize(frame->sun.xyz - input.worldPosition);
    int selfBody = input.instanceIndex < uint(frame->scene.x) ? int(input.instanceIndex) : -1;
    float shadow = selfBody < 0 ? beltExtinction(frame, input.worldPosition) * beltRockTransmittance(frame, input.worldPosition)
                                : (shadowMap ? localShadow(frame, input.worldPosition, s.N) : 1.0);
    s.visibility = solarVisibility(frame, input.worldPosition, selfBody) * shadow;
    return s;
}

float3 sunlight(Frame* frame) { return float3(1.0, .99, .97) * frame->sun.w; }
