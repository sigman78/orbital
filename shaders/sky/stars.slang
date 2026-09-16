#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"
#include "../lib/math.slang"
#include "../lib/projection.slang"

// The Bright Star Catalogue as points at infinity: one instanced quad per
// star, a Gaussian of about a pixel whose integrated energy is the star's
// real flux, so the sky's brightness distribution is the catalogue's (counts
// grow some three times per magnitude), the bright few bloom and the faint
// thousands stay faint. Drawn additively over the background before the
// bodies, which paint over them, and depth is left to the background.

static const float STAR_SIGMA = .7;      // pixels
static const float STAR_HALF_SIZE = 2.5; // quad half size in pixels
static const float STAR_ENERGY = 9.0;    // HDR radiance integrated over a magnitude 0 star at the panel's 1

struct StarVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 local : LOCAL;               // pixels from the centre
    [[vk::location(1)]] nointerpolation float3 tint : TINT;  // peak radiance
    [[vk::location(2)]] nointerpolation float sigma : SIGMA;
};

[shader("vertex")]
StarVaryings vertexMain(uint vertexIndex : SV_VertexID, uint instanceId : SV_InstanceID)
{
    StarVaryings o;
    o.position = float4(2.0, 2.0, 2.0, 1.0);
    o.local = float2(0.0, 0.0);
    o.tint = float3(0.0, 0.0, 0.0);
    o.sigma = STAR_SIGMA;
    Vertex star = root.vertices[instanceId]; // position: direction and flux; normal: colour and magnitude
    SkyParams sky = skyParams(root.frame);
    ViewParams view = viewParams(root.frame);
    float brightness = sky.starBrightness;
    if (brightness <= 0.0) return o;
    // At infinity: the direction alone, far along the ray; positions are camera-relative anyway.
    float4 clip = mul(viewProjection(root.frame), float4(star.position.xyz * (FAR_Z * .5), 1.0));
    if (clip.w <= NEAR_Z) return o;
    float2 resolution = view.size;
    float2 centre = (clip.xy / clip.w * .5 + .5) * resolution;
    float flux = star.position.w * STAR_ENERGY * brightness;
    // The brightest stars grow a little, so their size is a cue before the bloom is.
    float sigma = STAR_SIGMA + .25 * log2(max(flux / STAR_ENERGY, 1.0));
    float half = STAR_HALF_SIZE + 3.0 * (sigma - STAR_SIGMA);
    const float2 corners[6] = {float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, -1), float2(1, 1), float2(-1, 1)};
    float2 local = corners[vertexIndex] * half;
    float2 ndc = (centre + local) / resolution * 2.0 - 1.0;
    o.position = float4(ndc, clip.z / clip.w, 1.0);
    o.local = local;
    // The baked colour is the blackbody's; the eye sees stars nearly white, so the panel sets how much chroma shows.
    float3 colour = lerp(float3(1.0, 1.0, 1.0), star.normal.xyz, sky.starSaturation);
    o.tint = colour * flux / (2.0 * PI * sigma * sigma);
    o.sigma = sigma;
    return o;
}

[shader("fragment")]
float4 fragmentMain(StarVaryings i) : SV_Target0
{
    float r2 = dot(i.local, i.local);
    return float4(i.tint * exp(-r2 / (2.0 * i.sigma * i.sigma)), 0.0);
}
