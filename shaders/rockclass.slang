// Rock composition classes, shared by the culling pass (which colours the
// meshes and splats) and the disc bake (which splats the rocks' albedo into
// the far-belt map): silicate (S) rocks inside, dark carbonaceous (C) rocks
// outside, a few grey metallic (M) throughout. The class rides in the integer
// part of Instance.tint.w, the material seed in its fraction.
static const uint CLASS_S = 0, CLASS_C = 1, CLASS_M = 2;
static const float METAL_FRACTION = .03;

float hashId(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return float(x & 0xffffffu) / 16777216.0; }
float rockMaterialSeed(uint id) { return hashId(id * 2654435761u + 17u); }

uint rockClass(uint id, float radial)
{
    float carbon = lerp(.12, .85, smoothstep(0.0, 1.0, radial));
    float h = hashId(id * 747796405u + 5u);
    if (h < METAL_FRACTION) return CLASS_M;
    return h < METAL_FRACTION + carbon * (1.0 - METAL_FRACTION) ? CLASS_C : CLASS_S;
}

float3 rockColour(uint rockClass, uint id)
{
    // Telescope-grey: silicate rocks only faintly warm, carbonaceous ones charcoal, metallic ones cool grey.
    float3 base = rockClass == CLASS_S ? float3(.80, .76, .70) : rockClass == CLASS_C ? float3(.36, .36, .36) : float3(.70, .71, .74);
    float variation = .8 + .4 * hashId(id * 2891336453u + 3u);
    return base * variation;
}

// The scanned rock sets' mean albedo at their coarsest mip, as the splats
// use them: worn rock or boulder for silicate and metallic, rock face for
// carbonaceous. The disc bake cannot sample textures per vertex cheaply, so
// it takes these constants; keep them close to the sets' actual averages.
static const float3 ROCK_SET_AVERAGE_S = float3(.50, .47, .43);
static const float3 ROCK_SET_AVERAGE_C = float3(.42, .40, .38);

// Rocks up to this world radius make up the far-belt rock map and fade out
// of the splats as the map fades in; larger rocks are rare, would dominate
// their map texel, and keep drawing as splats and meshes at any distance.
static const float DISC_BAKE_MAX_RADIUS = .06;
