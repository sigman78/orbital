#pragma once
#include <array>
#include <cstddef>

namespace space::render {

// The timed sections of a frame. The groups come first, then the children of
// the surface, atmosphere and post groups, each one contiguous run of commands;
// a group's remainder over its children is the barriers and transitions between
// them. Order is the benchmark's column order and the panel's; recording order
// is free.
enum class GpuPass : unsigned {
    Frame,
    CullAndShadows,
    Culling,
    BodyShadows,
    BeltLight,
    BeltDiscs,
    Surface,
    Atmosphere,
    Post,
    SurfaceSky,    // the galaxy splat pass, the background and the stars
    SurfaceBodies, // the planet surfaces
    SurfaceRocks,  // the pooled rock multi-draw, meshes and splats together
    SurfaceClouds, // the cloud shell
    Atmospheres,   // the per-body shell marches
    BeltDust,      // the dust march, its upsample and the disc blend
    SplatMask,     // splat coverage and depth for TAA and sun visibility
    Temporal,      // the temporal resolve
    MotionStreaks,
    Bloom, // prefilter and both blurs
    SunVisibility,
    Flare,     // the quarter-resolution stack
    Composite, // tone map and the full-resolution lens work
    SpatialAA, // FXAA, or the three SMAA passes
    Meter,     // the exposure histogram's slice for the frame and the cycle's copies
    Present,   // the swapchain copy with the HUD and the panel
    Count
};

constexpr std::size_t gpu_pass_count = std::size_t(GpuPass::Count);

// What each pass is called in the benchmark CSV and the panel, and the group it
// belongs to (the frame is its own parent). The panel draws each group's
// children in proportion and the benchmark writes every pass, so a new scope
// needs a row here and nothing else.
struct GpuPassInfo {
    const char* column; // benchmark CSV column
    const char* label;  // panel
    GpuPass parent;
};

inline constexpr std::array<GpuPassInfo, gpu_pass_count> gpu_pass_info{{
    {"gpu_ms", "Frame", GpuPass::Frame},
    {"gpu_cull_shadow_ms", "Cull + shadows", GpuPass::Frame},
    {"gpu_cull_ms", "Culling", GpuPass::CullAndShadows},
    {"gpu_body_shadow_ms", "Body shadows", GpuPass::CullAndShadows},
    {"gpu_belt_light_ms", "Belt light", GpuPass::CullAndShadows},
    {"gpu_belt_disc_ms", "Belt discs", GpuPass::CullAndShadows},
    {"gpu_surface_ms", "Surface", GpuPass::Frame},
    {"gpu_atmosphere_ms", "Atmosphere", GpuPass::Frame},
    {"gpu_post_ms", "Post FX", GpuPass::Frame},
    {"gpu_surface_sky_ms", "Sky", GpuPass::Surface},
    {"gpu_surface_bodies_ms", "Bodies", GpuPass::Surface},
    {"gpu_surface_rocks_ms", "Rocks and splats", GpuPass::Surface},
    {"gpu_surface_clouds_ms", "Clouds", GpuPass::Surface},
    {"gpu_atmospheres_ms", "Atmospheres", GpuPass::Atmosphere},
    {"gpu_belt_dust_ms", "Belt dust", GpuPass::Atmosphere},
    {"gpu_splat_mask_ms", "Splat mask", GpuPass::Atmosphere},
    {"gpu_temporal_ms", "Temporal AA", GpuPass::Post},
    {"gpu_streaks_ms", "Motion streaks", GpuPass::Post},
    {"gpu_bloom_ms", "Bloom", GpuPass::Post},
    {"gpu_sun_visibility_ms", "Sun visibility", GpuPass::Post},
    {"gpu_flare_ms", "Flare stack", GpuPass::Post},
    {"gpu_composite_ms", "Composite", GpuPass::Post},
    {"gpu_spatial_aa_ms", "Spatial AA", GpuPass::Post},
    {"gpu_meter_ms", "Meter", GpuPass::Post},
    {"gpu_present_ms", "Present and UI", GpuPass::Post},
}};

constexpr const GpuPassInfo& pass_info(GpuPass pass) {
    return gpu_pass_info[std::size_t(pass)];
}

// Parents precede their children, so a walk in enum order meets every group
// before anything it contains.
constexpr bool gpu_pass_info_ordered() {
    for (std::size_t i = 1; i < gpu_pass_count; i++)
        if (std::size_t(gpu_pass_info[i].parent) >= i)
            return false;
    return gpu_pass_info[0].parent == GpuPass::Frame;
}
static_assert(gpu_pass_info_ordered());

// A frame's pass timings in milliseconds; zero where a pass did not record.
struct PassTimings {
    std::array<float, gpu_pass_count> ms{};
    constexpr float& operator[](GpuPass pass) { return ms[std::size_t(pass)]; }
    constexpr float operator[](GpuPass pass) const { return ms[std::size_t(pass)]; }
};

} // namespace space::render
