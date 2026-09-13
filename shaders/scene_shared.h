#pragma once
#include "resource_slots.h"
#ifdef __cplusplus
#include <cstdint>
struct alignas(16) ShaderFloat4 { float x, y, z, w; };
using ShaderMatrix4 = float[16];
#define SHADER_FLOAT4 ShaderFloat4
#define SHADER_MATRIX4 ShaderMatrix4
#define SHADER_ADDRESS std::uint64_t
#define SHADER_UINT std::uint32_t
#else
#define SHADER_FLOAT4 float4
#define SHADER_MATRIX4 float4x4
#define SHADER_ADDRESS(T) T*
#define SHADER_UINT uint
#endif

// Root.mode of the surface pipelines (SurfaceMode on the C++ side).
#define ORBITAL_SURFACE_OPAQUE 0
#define ORBITAL_SURFACE_CLOUD 1
#define ORBITAL_SURFACE_SHADOW 2
#define ORBITAL_SURFACE_BILLBOARD 3
#define ORBITAL_SURFACE_SPLAT_MASK 4
// Instance.rotation_kind.w (SurfaceKind on the C++ side); each kind has its own fragment shader.
#define ORBITAL_KIND_EARTH 0
#define ORBITAL_KIND_GIANT 1
#define ORBITAL_KIND_MOON 2
#define ORBITAL_KIND_ROCK 3
#define ORBITAL_KIND_MARS 4

struct Instance { SHADER_FLOAT4 center_radius; SHADER_FLOAT4 rotation_kind; SHADER_FLOAT4 tint; };
struct Vertex { SHADER_FLOAT4 position; SHADER_FLOAT4 normal; };
struct Frame {
    SHADER_MATRIX4 view_projection;
    SHADER_FLOAT4 camera_time, right_tan, up_aspect, forward_exposure, sun;
    SHADER_FLOAT4 bodies[8];               // camera-relative centre and radius; unused entries have radius 0
    SHADER_FLOAT4 options, screen_sun;
    SHADER_MATRIX4 light_projection;
    SHADER_MATRIX4 previous_projection;
    SHADER_FLOAT4 previous_camera_delta, previous_forward, jitter;
    SHADER_FLOAT4 belt_ring;   // inner radius, outer radius, density, thickness (giant-relative units)
    SHADER_FLOAT4 belt_normal; // unit normal of the belt plane, w unused
    SHADER_FLOAT4 scene;       // body count, giant body index, belt transmittance map on, belt extinction on
    SHADER_MATRIX4 belt_light_projection; // orthographic box over the whole belt from the sun
    SHADER_FLOAT4 belt_light;  // box half width, half height, map texels, half span of the slice range along the light
    SHADER_FLOAT4 quality;     // temporal anti-aliasing on, tone curve (0 ACES filmic, 1 AgX, 2 PBR Neutral), belt dust on, unused
    SHADER_FLOAT4 dust;        // belt dust multipliers: density (extinction), brightness (albedo), far-view scale, saturation
    SHADER_FLOAT4 dust_tint;   // multiplies the dust colour, w unused
    SHADER_MATRIX4 belt_disc_projection; // orthographic box over the belt plane, for the far-belt maps
    SHADER_FLOAT4 belt_disc_right;       // disc frame right axis, w = half extent
    SHADER_FLOAT4 belt_disc_up;          // disc frame up axis, w = half extent
    SHADER_FLOAT4 belt_disc;             // sunlight map texels, far-tier blend weight (0 near paths, 1 baked disc), rock map texels, disc LOD enabled
    SHADER_FLOAT4 earth;                 // ocean roughness, glint intensity, cloud shadow strength, cloud shadow softness (mip bias)
    SHADER_FLOAT4 earth_more;            // sea wind patchiness, cloud opacity, unused x2
    SHADER_FLOAT4 lens;                  // glare intensity, ghost strength, starburst strength, starburst blades
    SHADER_FLOAT4 sun_disc;              // angular radius (radians), limb darkening, unused x2
    SHADER_FLOAT4 post;                  // bloom intensity (0 off), bloom threshold, bloom knee, chromatic aberration scale
    SHADER_FLOAT4 post_more;             // vignette, film grain, black offset of the neutral tone curve, unused
    SHADER_FLOAT4 giant;                 // flow time scale (x real), advection cycle (seconds), turbulence, flow map on
    SHADER_FLOAT4 giant_more;            // limb haze optical depth, terminator wrap, cloud relief exaggeration, polar cap opacity
    SHADER_FLOAT4 giant_night;           // lightning flashes per second, lightning brightness, polar cap size (0 off), polar cap blend width
    SHADER_FLOAT4 giant_layers;          // upper deck lift (texture units at 45 degrees), upper deck shadow, close-range streak strength (0 off), unused
    SHADER_FLOAT4 camera_lattice;        // the camera's offset within its mote cell, cell size
    SHADER_FLOAT4 camera_cell;           // the camera's mote cell index, motion streak intensity (0 off)
    SHADER_FLOAT4 stars;                 // catalogue stars loaded and on, star brightness (0 off), star colour saturation, Milky Way brightness (0: the procedural band)
    SHADER_FLOAT4 galaxy;                // Milky Way splats drawn (the first n of the cloud), dust fBm amplitude (0 off), base feature size in degrees, galaxy mode (0 splats, 1 layers, 2 original)
    SHADER_FLOAT4 galaxy_layers;         // low-frequency, cloud and filament gains, unused
    SHADER_FLOAT4 galaxy_more;           // dust fBm lacunarity, gain, the galaxy pass's resolution divisor, band contrast exponent
};
struct Root {
#ifdef __cplusplus
    SHADER_ADDRESS frame, vertices, instances;
#else
    SHADER_ADDRESS(Frame) frame;
    SHADER_ADDRESS(Vertex) vertices;
    SHADER_ADDRESS(Instance) instances;
#endif
    SHADER_UINT base, mode;
};

// --- GPU belt culling (shaders/belt/cull.slang) ----------------------------------

#define ORBITAL_ROCK_LEVELS 6                            // geometry::rock_level_count
#define ORBITAL_ROCK_GROUPS (16 * ORBITAL_ROCK_LEVELS)   // geometry::rock_shape_count * levels; group index = shape * levels + level
#define ORBITAL_BELT_BANDS 8                             // belt::radial_bands
#define ORBITAL_CULL_THREADS 128

// Static per-rock record in the belt's own frame (before spin and tilt).
struct RockData {
    SHADER_FLOAT4 position_radius; // belt-local centre, world radius
    SHADER_FLOAT4 rotation_seed;   // Euler angles at t = 0, w unused
    SHADER_FLOAT4 spin_group;      // tumble rate per axis, w = shape * ORBITAL_BELT_BANDS + band
};

// VkDrawIndexedIndirectCommand padded to 32 bytes. The billboard entry holds a
// VkDrawIndirectCommand in its first four words instead.
struct DrawArgs {
    SHADER_UINT index_count, instance_count, first_index, vertex_offset, first_instance, pad0, pad1, pad2;
};

struct CullParams {
    SHADER_FLOAT4 right, up, forward;   // camera basis; w = tan_x, tan_y, pixel padding per unit depth
    SHADER_FLOAT4 view;                 // pixels per unit depth, plane x scale, plane y scale, rock spin angle
    SHADER_FLOAT4 giant;                // camera-relative belt centre, w unused
    SHADER_FLOAT4 belt_tilt;            // y scale, y from z, z scale, unused
    SHADER_FLOAT4 band_spin[ORBITAL_BELT_BANDS]; // cos and sin of each radial band's spin angle
    SHADER_FLOAT4 levels;               // projected-radius thresholds of levels 1 to 4, in pixels
    SHADER_FLOAT4 billboard;            // level 5 threshold, billboard radius, minimum radius, far-tier blend weight (splats fade out by it)
    SHADER_UINT rock_limit, body_count, light_in_count_pass, unused; // light_in_count_pass: splats lit in both passes (development comparison)
    // Each rock group's slice of the pooled rock mesh.
    SHADER_UINT index_counts[ORBITAL_ROCK_GROUPS];
    SHADER_UINT first_indices[ORBITAL_ROCK_GROUPS];
    SHADER_UINT vertex_offsets[ORBITAL_ROCK_GROUPS];
};

// Per-frame scratch the culling passes read and write; the CPU fills params,
// the instance pointer and zeroes the counters before each frame.
struct CullScratch {
    CullParams params;
#ifdef __cplusplus
    SHADER_ADDRESS instances;
    SHADER_UINT counts[ORBITAL_ROCK_GROUPS + 1], cursors[ORBITAL_ROCK_GROUPS + 1];
#else
    SHADER_ADDRESS(Instance) instances;
    Atomic<uint> counts[ORBITAL_ROCK_GROUPS + 1];
    Atomic<uint> cursors[ORBITAL_ROCK_GROUPS + 1];
#endif
    SHADER_UINT draw_count, pad0, pad1, pad2; // non-empty rock groups compacted to the front of args
    DrawArgs args[ORBITAL_ROCK_GROUPS + 1];   // the billboard entry stays at index ORBITAL_ROCK_GROUPS
};

// The belt splat pass reuses this root with rocks pointing at the size-tail
// records and pass holding their count.
struct CullRoot {
#ifdef __cplusplus
    SHADER_ADDRESS frame, rocks, scratch;
#else
    SHADER_ADDRESS(Frame) frame;
    SHADER_ADDRESS(RockData) rocks;
    SHADER_ADDRESS(CullScratch) scratch;
#endif
    SHADER_UINT pass, unused;
};

#undef SHADER_FLOAT4
#undef SHADER_MATRIX4
#undef SHADER_ADDRESS
#undef SHADER_UINT
