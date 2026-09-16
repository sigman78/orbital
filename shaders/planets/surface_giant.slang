#include "../scene/frame_bindings.slang"
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
    Material m = giantMaterial(root.frame, input, rotate, s.N, s.L, s.V, s.visibility, root.detail);
    float2 uv = sphereUV(normalize(input.localNormal));
    float shear = sampleSphere(textures[TEX_GAS_FLOW], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).b;
    return float4(shadeGiant(root.frame, m, m.N, s.N, s.L, s.V, uv, shear), 1);
}
