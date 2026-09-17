#pragma once
#include "../scene_shared.h"
#include "frame_records.slang"
#include "../belt/rockclass.slang"

// The draw indices still count bodies first, then compacted asteroids.
// Only the storage stride changes at the body/asteroid boundary.
Instance loadInstance(Frame* frame, Instance* instances, uint index, bool billboard)
{
    uint bodies = uint(bodyCount(frame));
    if (index < bodies) return instances[index];
    AsteroidInstance* asteroids = (AsteroidInstance*)(instances + bodies);
    AsteroidInstance packed = asteroids[index - bodies];
    Instance result;
    result.center_radius = packed.center_radius;
    result.previous_center = float4(0.0, 0.0, 0.0, 0.0);
    result.previous_rotation = float4(0.0, 0.0, 0.0, 0.0);
    if (billboard)
    {
        uint br = packed.payload[1];
        float2 rg = unpackHalf2x16ToFloat(packed.payload[0]);
        float2 blueRim = unpackHalf2x16ToFloat(br & 0x7fffffffu);
        result.rotation_kind = float4((br & 0x80000000u) != 0 ? 1.0 : 0.0,
                                      blueRim.y,
                                      asfloat(packed.payload[2]), ORBITAL_KIND_ROCK);
        result.tint = float4(rg.x, rg.y, blueRim.x, asfloat(packed.payload[3]));
    }
    else
    {
        uint id = packed.payload[3] & 0x3fffffffu;
        uint composition = packed.payload[3] >> 30;
        result.rotation_kind = float4(asfloat(packed.payload[0]), asfloat(packed.payload[1]),
                                      asfloat(packed.payload[2]), ORBITAL_KIND_ROCK);
        result.tint = float4(rockColour(composition, id), float(composition) + rockMaterialSeed(id));
        float2 dxy = unpackHalf2x16ToFloat(packed.previous[0]), dzr = unpackHalf2x16ToFloat(packed.previous[1]),
               drot = unpackHalf2x16ToFloat(packed.previous[2]);
        result.previous_center = float4(dxy.x, dxy.y, dzr.x, 0.0);
        result.previous_rotation = float4(dzr.y, drot.x, drot.y, 0.0);
    }
    return result;
}
