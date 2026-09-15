#pragma once
static const float CLOUD_HEIGHT = ORBITAL_CLOUD_HEIGHT;  // cloud shell above the surface, in radii (matches the 1.009 mesh scale)

// Direction on the cloud shell that the sun ray from a point at this altitude
// (radii above the surface, in the body's frame) passes through.
float3 cloudShellDirection(float3 localDirection, float altitude, float3 localLight)
{
    float rise = max(CLOUD_HEIGHT - altitude, 0.0);
    return normalize(localDirection + localLight * (rise / max(dot(localDirection, localLight), .25)));
}

