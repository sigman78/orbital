#include "earth_material.slang"

// The Earth's surface pipeline: the ground in the opaque mode, the cloud shell in
// the cloud mode; the material and shading are in earth_material.slang, which the
// Earth chart draws through the same functions.

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Surface s = surfaceAt(root.frame, root.instances, input, true);
    float3 localN = normalize(input.localNormal);
    if (root.mode == MODE_CLOUD)
        return shadeClouds(localN, s.N, s.L, s.visibility, max(dot(s.N, s.L), 0.0), root.detail);
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    return float4(earthGround(localN, rotate, s.N, s.L, s.V, s.visibility, root.detail, root.flags), 1);
}
