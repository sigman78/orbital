#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../scene/shadows.slang"
#include "../surface/lighting.slang"
#include "giant_material.slang"
#include "giant_lighting.slang"
#include "../lib/math.slang"
#include "../lib/sampling.slang"

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Surface s = surfaceAt(root.frame, root.instances, input, false);
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    GiantParams giant = giantParams(root.frame);
    float time = viewParams(root.frame).time;
    float visibility = s.visibility * beltShadow(root.frame, input.worldPosition);
    Material m = giantMaterial(giant, time, input, rotate, s.N, s.L, s.V, visibility, root.detail);
    float2 uv = sphereUV(normalize(input.localNormal));
    float shear = sampleSphere(textures[TEX_GAS_FLOW], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).b;
    return float4(shadeGiant(giant, lightParams(root.frame), time, m, m.N, s.N, s.L, s.V, uv, shear), 1);
}
