#pragma once
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
    SHADER_FLOAT4 scene;       // body count, giant body index, unused, unused
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

// --- GPU belt culling (shaders/cull.slang) ----------------------------------

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
    SHADER_FLOAT4 billboard;            // level 5 threshold, billboard radius, minimum radius, unused
    SHADER_UINT rock_limit, body_count, unused0, unused1;
    SHADER_UINT index_counts[ORBITAL_ROCK_GROUPS];
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
    DrawArgs args[ORBITAL_ROCK_GROUPS + 1];
};

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
