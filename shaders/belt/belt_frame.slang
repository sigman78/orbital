#pragma once
#include "../scene_shared.h"

// The belt frame to the giant's frame: the CPU sweep hands over each rock's
// position in the belt frame, and the tilt is this fixed rotation about X with
// the plane compressed.
float3 beltTilt(float3 v)
{
    return float3(v.x, v.y * ORBITAL_BELT_TILT_Y_SCALE - v.z * ORBITAL_BELT_TILT_Y_FROM_Z, v.z * ORBITAL_BELT_TILT_Z_SCALE);
}

// A rock's belt-relative position from its state (x, y, z, phase).
float3 rockPosition(float4 state)
{
    return beltTilt(state.xyz);
}

// A rock's Euler angles: the seed plus the phase along the tumble axis.
float3 rockRotation(float4 state, RockData rock)
{
    return rock.rotation_seed.xyz + rock.spin_group.xyz * state.w;
}
