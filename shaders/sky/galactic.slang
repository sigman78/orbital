#pragma once
#include "../lib/math.slang"

// J2000 equatorial xyz to galactic xyz. Orbital's sky frame uses y for the pole.
float3 galacticDirection(float3 ray)
{
    const float3x3 rotation = float3x3(-.054876, -.873437, -.483835,
                                      .494109, -.444830, .746982,
                                      -.867666, -.198076, .455984);
    return mul(rotation, float3(ray.x, ray.z, ray.y));
}

float2 galacticUV(float3 direction)
{
    return float2(.5 - atan2(direction.y, direction.x) / (2.0 * PI),
                  .5 - asin(clamp(direction.z, -1.0, 1.0)) / PI);
}
