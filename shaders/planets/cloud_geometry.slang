#pragma once
#include "../scene_shared.h"
static const float CLOUD_HEIGHT = ORBITAL_CLOUD_HEIGHT;  // cloud shell above the surface, in radii (matches the 1.009 mesh scale)

// Direction on the cloud shell that the view ray through a ground point crosses,
// toward the viewer: the clouds the shell would show over that pixel.
float3 cloudShellView(float3 localDirection, float3 localView)
{
    float along = dot(localDirection, localView);
    float t = -along + sqrt(max(along * along + 2.0 * CLOUD_HEIGHT + CLOUD_HEIGHT * CLOUD_HEIGHT, 0.0));
    return normalize(localDirection + localView * t);
}

// Direction on the cloud shell that the sun ray from a point at this altitude
// (radii above the surface, in the body's frame) passes through; past the
// terminator the tangent offset keeps the sample local.
float3 cloudShellDirection(float3 localDirection, float altitude, float3 localLight)
{
    float r = 1.0 + clamp(altitude, 0.0, CLOUD_HEIGHT), shell = 1.0 + CLOUD_HEIGHT;
    float along = r * max(dot(localDirection, localLight), 0.0);
    float t = -along + sqrt(max(along * along + shell * shell - r * r, 0.0));
    return normalize(localDirection * r + localLight * t);
}
