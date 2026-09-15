#include "../scene/frame_bindings.slang"
#include "../surface/lighting.slang"
#include "../surface/brdf.slang"
#include "../lib/math.slang"
#include "../lib/noise.slang"
#include "../lib/sampling.slang"
#include "clouds.slang"

// The Earth: day map with land grain, the sea's glint, the sourced normal map,
// cloud shadows on the ground, city lights past the terminator; and the cloud
// shell drawn over it in the cloud mode.
//
// Draw tier (root.detail, set per draw from the projected size): the normal map
// and the cloud swirl fade out toward the smallest mesh level, with their work
// skipped at zero, while the lighting terms stay as they are at every size.
// When the shell is not drawn (root.flags, small or far bodies, where the shell
// sits within a few depth steps of the ground and fights it) the ground pass
// blends the clouds in itself, lit as the shell would be, with no parallax.

static const float EARTH_NORMAL_Y = 1.0;      // the sourced map is DirectX-style (green = south), matching B below
static const float2 EARTH_TEXELS = float2(4096.0, 2048.0);

// --- Clouds (the layer's coordinate and shadow helpers are in clouds.slang) ---

// Cloud coverage with fine structure added where the 4K mask is magnified.
float cloudDensity(float2 uv, float t, float detail)
{
    float density = sampleSphere(textures[TEX_EARTH_CLOUDS], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).a;
    density *= 1.0 + (fbm2(uv * float2(1024, 512) + t * .01, 3) - .5) * .6 * detail;
    return pow(saturate(density), 1.3);
}

float4 shadeClouds(SurfaceVaryings input, float3 N, float3 L, float visibility, float nl)
{
    float t = root.frame->camera_time.w;
    float2 uv = sphereUV(normalize(input.localNormal));
    float alpha = cloudDensity(cloudUV(uv, t, root.detail), t, magnification(uv, EARTH_TEXELS)) * root.frame->earth_more.y;
    // The clouds' starlight fill follows the panel's ambient fill, as the ground's does.
    float3 cloudLight = float3(.002, .004, .008) * root.frame->quality.w + float3(1.0, .99, .97) * pow(nl, .8) * visibility * root.frame->sun.w / PI * .8;
    return float4(cloudLight, alpha);
}

// --- Ground -----------------------------------------------------------------------

Material earthMaterial(SurfaceVaryings input, float3x3 rotate, float3 N, float3 L, float visibility)
{
    float t = root.frame->camera_time.w;
    float3 localN = normalize(input.localNormal);
    float2 uv = sphereUV(localN);
    float detail = magnification(uv, EARTH_TEXELS);
    Material m;
    m.albedo = sampleSphere(textures[TEX_EARTH_ALBEDO], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).rgb;
    m.water = sampleSphere(textures[TEX_EARTH_SPECULAR], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).r;
    // Land grain where the 4K map is magnified; the sea stays smooth.
    m.albedo *= 1.0 + (fbm2(uv * float2(8192, 4096), 3) - .5) * .3 * detail * (1.0 - m.water * .7);
    // Sea state: the panel's roughness, patchy with a slow wind field so the glint
    // breaks into brighter and calmer stretches instead of one even highlight.
    float4 earth = root.frame->earth;
    float wind = 1.0 + (fbm2(uv * float2(48, 24) + t * .0004, 2) - .5) * root.frame->earth_more.x;
    m.rough = lerp(.85, max(earth.x * wind, .03), m.water);
    // The normal map's tilt fades with the draw tier toward the geometric normal.
    m.N = N;
    if (root.detail > 0.0) {
        float3 T, B;
        tangentFrame(localN, rotate, N, T, B);
        float3 mapN = sampleSphere(textures[TEX_EARTH_NORMAL], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).xyz * 2.0 - 1.0;
        mapN.y *= EARTH_NORMAL_Y;
        float tilt = .55 * root.detail;
        m.N = normalize(T * mapN.x * tilt + B * mapN.y * tilt + N * lerp(1.0, max(mapN.z, .2), root.detail));
    }
    // Cloud shadow: the cloud above this point along the sun direction.
    float3 localL = mul(transpose(rotate), L);
    float3 towardSun = cloudShellDirection(localN, 0.0, localL);
    float shadowCloud = cloudShadowDensity(cloudUV(sphereUV(towardSun), t, root.detail), earth.w);
    m.visibility = visibility * (1.0 - shadowCloud * earth.z);
    return m;
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Surface s = surfaceAt(root.frame, root.instances, input, true);
    if (root.mode == MODE_CLOUD) return shadeClouds(input, s.N, s.L, s.visibility, max(dot(s.N, s.L), 0.0));
    float3x3 rotate = rotation(s.inst.rotation_kind.xyz);
    Material m = earthMaterial(input, rotate, s.N, s.L, s.visibility);
    float3 colour = shadeSurface(m, s.L, s.V, sunlight(root.frame), root.frame->earth.y, root.frame->quality.w);
    // City lights come up past the terminator. The night map carries a blue
    // floor over the whole globe (4 to 16 sRGB codes with faint cloud and land
    // texture in it; linear below) that the neutral curve keeps at full
    // strength, since its offset takes the darkest channel. The floor is
    // subtracted and what is left below the lights' level gated out, so the
    // night side is black but for the lights.
    const float3 NIGHT_FLOOR = float3(.0045, .0055, .014);
    float2 uv = sphereUV(normalize(input.localNormal));
    float3 lights = max(sampleSphere(textures[TEX_EARTH_NIGHT], samplers[SAMPLER_WRAP_U_ANISOTROPIC], uv).rgb - NIGHT_FLOOR, 0.0) / (1.0 - NIGHT_FLOOR);
    lights *= smoothstep(0.0, .008, max(lights.r, max(lights.g, lights.b)));
    colour += lights * pow(1 - smoothstep(-.15, .1, dot(m.N, s.L)), 2.0) * .65;
    // The clouds folded in where the shell is not drawn: what the shell's alpha
    // blend would have composited over this pixel, from the ground's own normal
    // and the visibility before the cloud's shadow on the ground.
    if (root.flags & ORBITAL_ROOT_FOLD_CLOUDS) {
        float4 clouds = shadeClouds(input, s.N, s.L, s.visibility, max(dot(s.N, s.L), 0.0));
        colour = lerp(colour, clouds.rgb, clouds.a);
    }
    return float4(colour, 1);
}
