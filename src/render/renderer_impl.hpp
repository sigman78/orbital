#pragma once
// Private renderer state: one owner for shared GPU resources. Implementation
// units separate frame preparation, assets, pipelines, and pass families.
#include "render/renderer.hpp"

#include "assets/image.hpp"
#include "assets/texture.hpp"
#include "core/types.hpp"
#include "post/bloom_shared.h"
#include "render/gpu_commands.hpp"
#include "render/gpu_image.hpp"
#include "render/gpu_owners.hpp"
#include "render/gpu_sync.hpp"
#include "render/gpu_timing.hpp"
#include "render/gpu_types.hpp"
#include "render/showcase.hpp"
#include "scene/geometry.hpp"
#include "scene/system.hpp"

#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

namespace space::render {

// --- Fixed layout shared with the shaders -------------------------------------

// Sampled texture descriptor slots; initialize the entire shader array, including reserved slots.
enum class Slot : unsigned {
    hdr = TEX_HDR,
    bloom_a = TEX_BLOOM_A,
    bloom_b = TEX_BLOOM_B,
    earth_albedo = TEX_EARTH_ALBEDO,
    gas_albedo = TEX_GAS_ALBEDO,
    earth_clouds = TEX_EARTH_CLOUDS,
    final_image = TEX_FINAL_IMAGE,
    earth_normal = TEX_EARTH_NORMAL,
    earth_specular = TEX_EARTH_SPECULAR,
    earth_night = TEX_EARTH_NIGHT,
    moon_albedo = TEX_MOON_ALBEDO,
    rock_albedo = TEX_ROCK_ALBEDO,
    rock_normal = TEX_ROCK_NORMAL,
    rock_roughness = TEX_ROCK_ROUGHNESS,
    hud = TEX_HUD,
    shadow_map = TEX_SHADOW_MAP,
    history_a = TEX_HISTORY_A,
    history_b = TEX_HISTORY_B,
    depth = TEX_DEPTH,
    mars_albedo = TEX_MARS_ALBEDO,
    mars_normal = TEX_MARS_NORMAL,
    moon_normal = TEX_MOON_NORMAL,
    rock_face_albedo =
        TEX_ROCK_FACE_ALBEDO, // second and third rock sets follow the first's albedo, normal+height, roughness order
    rock_face_normal = TEX_ROCK_FACE_NORMAL,
    rock_face_roughness = TEX_ROCK_FACE_ROUGHNESS,
    rock_boulder_albedo = TEX_ROCK_BOULDER_ALBEDO,
    rock_boulder_normal = TEX_ROCK_BOULDER_NORMAL,
    rock_boulder_roughness = TEX_ROCK_BOULDER_ROUGHNESS,
    belt_light_map = TEX_BELT_LIGHT_MAP,   // belt transmittance, blurred
    belt_light_blur = TEX_BELT_LIGHT_BLUR, // scratch between the two blur directions
    ldr = TEX_LDR,                         // tone-mapped image before FXAA
    splat_mask = TEX_SPLAT_MASK,           // splat coverage-weighted linear depth (R) and coverage (A)
    smaa_area = TEX_SMAA_AREA,             // SMAA area lookup, 160x560
    smaa_search = TEX_SMAA_SEARCH,         // SMAA search lookup, 64x16
    smaa_edges = TEX_SMAA_EDGES,           // SMAA edges of the tone-mapped image
    smaa_weights = TEX_SMAA_WEIGHTS,       // SMAA blending weights
    ui_font = TEX_UI_FONT,                 // Dear ImGui font atlas
    belt_dust = TEX_BELT_DUST,             // half-resolution belt dust march, composited with a depth-aware upsample
    belt_disc_light = TEX_BELT_DISC_LIGHT, // far-belt map: sunlight reaching the belt plane, baked every few frames
    belt_disc_rocks = TEX_BELT_DISC_ROCKS, // far-belt map: rock coverage times albedo, splatted every few dozen frames
    gas_flow = TEX_GAS_FLOW,               // gas giant wind flow map, baked by tools/bake-flow-map.py
    gas_detail = TEX_GAS_DETAIL,           // gas giant flow-aligned fine detail, baked alongside it
    gas_relief = TEX_GAS_RELIEF,           // gas giant cloud-top slopes and height, baked alongside it
    gas_polar = TEX_GAS_POLAR,             // gas giant polar cap albedo atlas (north above south), baked alongside it
    gas_polar_flow = TEX_GAS_POLAR_FLOW,   // gas giant polar cap flow atlas
    milky_way = TEX_MILKY_WAY,             // screen-space galaxy splat sum
    galaxy_low = TEX_GALAXY_LOW,
    galaxy_clouds = TEX_GALAXY_CLOUDS,
    galaxy_filaments = TEX_GALAXY_FILAMENTS,
    galaxy_original = TEX_GALAXY_ORIGINAL,
    sun_visibility = TEX_SUN_VISIBILITY,
    flare = TEX_FLARE,         // the soft lens flare stack at a fraction of the frame
    lens_dirt = TEX_LENS_DIRT, // dirty-glass mask stretched over the frame, baked by tools/bake-lens-dirt.py
    count = ORBITAL_TEXTURE_COUNT,
};

// Sampler descriptor slots (shaders/scene/bindings.slang declares the array).
enum class SamplerSlot : unsigned {
    clamp = SAMPLER_CLAMP,
    wrap_u_anisotropic = SAMPLER_WRAP_U_ANISOTROPIC,
    shadow_compare = SAMPLER_SHADOW_COMPARE,
    wrap_anisotropic = SAMPLER_WRAP_ANISOTROPIC,
    count = ORBITAL_SAMPLER_COUNT
};

// Root.mode as interpreted by surface.slang.
enum class SurfaceMode : std::uint32_t {
    opaque = ORBITAL_SURFACE_OPAQUE,
    cloud = ORBITAL_SURFACE_CLOUD,
    shadow = ORBITAL_SURFACE_SHADOW,
    billboard = ORBITAL_SURFACE_BILLBOARD,
    splat_mask = ORBITAL_SURFACE_SPLAT_MASK
};
// Only bloom selects a stage through Root.mode; other post pipelines have dedicated entry points.
enum class BloomMode : std::uint32_t {
    prefilter = ORBITAL_BLOOM_PREFILTER,
    horizontal = ORBITAL_BLOOM_HORIZONTAL,
    vertical = ORBITAL_BLOOM_VERTICAL
};

// Instance.rotation_kind.w as interpreted by surface.slang and atmosphere.slang.
enum class SurfaceKind : unsigned {
    earth = ORBITAL_KIND_EARTH,
    giant = ORBITAL_KIND_GIANT,
    moon = ORBITAL_KIND_MOON,
    rock = ORBITAL_KIND_ROCK,
    mars = ORBITAL_KIND_MARS
};

constexpr SurfaceKind surface_kind(BodyClass body_class) {
    switch (body_class) {
    case BodyClass::Terrestrial: return SurfaceKind::earth;
    case BodyClass::GasGiant: return SurfaceKind::giant;
    case BodyClass::RockyMoon: return SurfaceKind::moon;
    case BodyClass::Desert: return SurfaceKind::mars;
    case BodyClass::Moonlet: return SurfaceKind::rock;
    }
    return SurfaceKind::rock;
}

// --- Shared heap layout and limits ------------------------------------------

// CPU-written static data and a small per-frame upload region are host-visible.
// Culling counters and generated instances live in a separate device-only heap.
struct HeapLayout {
    std::uint64_t dynamic_offset = 80ull << 20; // static mesh/rock records before this point
    std::uint64_t cull_offset = 1280;           // FrameData (1072 bytes, padded), then culling parameters/counters
    std::uint64_t instance_offset = 1280 + 8192;
    std::uint64_t staging_budget = 64ull << 20; // bounded texture upload staging
    std::uint64_t ui_bytes = 4ull << 20;
    std::uint64_t instance_budget = (44ull << 20) - instance_offset; // retain the former instance limit

    constexpr std::uint64_t ui_offset() const { return instance_offset + max_body_count * sizeof(Instance); }
    constexpr std::uint64_t mapped_size() const { return dynamic_offset + ui_offset() + ui_bytes; }
    constexpr std::uint64_t instance_capacity() const { return instance_budget / sizeof(Instance); }
    constexpr std::uint64_t cull_size(unsigned rocks, unsigned bodies) const {
        return instance_offset + std::uint64_t(bodies) * sizeof(Instance) +
               std::uint64_t(rocks) * sizeof(AsteroidInstance);
    }
};
inline constexpr HeapLayout heap_layout{};
static_assert(heap_layout.instance_capacity() < (1ull << 30)); // packed asteroid id
static_assert(heap_layout.cull_offset >= sizeof(FrameData));
static_assert(heap_layout.instance_offset >= heap_layout.cull_offset + sizeof(CullScratch));
static_assert(heap_layout.instance_offset < heap_layout.ui_offset());

// Sizes of the fixed GPU targets, created by the resources side and addressed by the frame side.
namespace targets {
inline constexpr Range<float> depth{0.02f, 2000.0f}; // near and far plane, camera-relative units
inline constexpr unsigned shadow_map_size = 2048;
inline constexpr unsigned belt_light_map_size = 2048;    // the whole belt in light space, rgba16f coverage slices
inline constexpr unsigned belt_disc_map_size = 1024;     // the whole belt over its plane, for the far tier: sunlight
inline constexpr unsigned belt_disc_rock_map_size = 256; // rock coverage, coarse so each texel averages many rocks
inline constexpr unsigned belt_disc_light_interval = 16, belt_disc_rock_interval = 32; // frames between bakes
inline constexpr unsigned meter_size = 16;             // luminance meter edge, texels of rgba32f
inline constexpr unsigned mote_cell_size_units = 5;    // motion streak lattice cell, scene units
inline constexpr unsigned mote_count = 4 * 4 * 4 * 16; // MOTE_CELLS^3 * MOTES_PER_CELL in motes.slang
} // namespace targets

// What FrameInput::high_quality selects between.
struct QualityTier {
    unsigned belt_count;
};
inline constexpr QualityTier baseline_quality{.belt_count = 280000};
inline constexpr QualityTier high_quality{.belt_count = 520000};

// Belt rock sizing, needed when clusters are bounded and again when rocks are culled.
namespace belt {
inline constexpr float rock_radius_scale = 0.025f; // instance scale to world radius
inline constexpr unsigned radial_bands = 8;        // cluster bins across the belt width; each orbits at its own rate
inline constexpr float map_caster_min_radius =
    .03f; // only the size-tail rocks (about 5%) splat into the transmittance map

} // namespace belt

// --- GPU-side records ---------------------------------------------------------

struct GpuMesh {
    std::uint64_t vertices = 0, indices = 0; // GPU addresses in the static heap
    unsigned index_count = 0;
    unsigned first_index = 0, vertex_offset = 0; // position inside a pooled buffer, zero for standalone meshes
};

enum class Blend { none, alpha, additive, premultiplied };

struct PipelineDesc {
    const char* vertex_shader;   // shaders/<name>.vertex.spv
    const char* fragment_shader; // shaders/<name>.fragment.spv
    gpu::Format color_format;
    bool has_depth_attachment = false;
    Blend blend = Blend::none;
};

inline gpu::Format texture_format(const assets::TextureData& data) {
    if (data.format == assets::TextureFormat::RGBA8)
        return gpu::Format::rgba8_unorm;
    if (data.format == assets::TextureFormat::BC7)
        return gpu::Format::bc7_unorm;
    switch (data.block_x) {
    case 6: return gpu::Format::astc_6x6_unorm;
    case 8: return gpu::Format::astc_8x8_unorm;
    case 12: return gpu::Format::astc_12x12_unorm;
    default: return gpu::Format::astc_4x4_unorm;
    }
}

struct Upload {
    assets::TextureData data;
    Slot slot;
};

// One draw group per (shape, level) pair of the rock library; the GPU
// culling pass appends the billboard list as one more group.
constexpr unsigned rock_group_count = geometry::rock_shape_count * geometry::rock_level_count;
constexpr unsigned rock_group(unsigned shape, unsigned level) {
    return shape * geometry::rock_level_count + level;
}
static_assert(rock_group_count == ORBITAL_ROCK_GROUPS && geometry::rock_level_count == ORBITAL_ROCK_LEVELS &&
              belt::radial_bands == ORBITAL_BELT_BANDS);

struct Renderer::Impl {
    // Device and heaps.
    gpu::Device* device = nullptr;
    SubmissionTimeline submissions;
    struct BufferResources {
        UniqueGpuHeap data, texture_descriptors, sampler_descriptors, luminance_readback;
        UniqueGpuHeap cull_device, cull_readback; // GPU output and completed scratch for CPU statistics
    } buffers;
    std::uint64_t static_cursor = 0;

    // Resources.
    std::vector<GpuImage> material_images;
    std::vector<UniquePso> pipelines;
    FrameTargets frame_targets;
    FixedTargets fixed_targets;
    // Non-owning handles grouped by pass family; pipelines owns their lifetime.
    struct {
        struct {
            gpu::PSO* surface_earth = nullptr;
            gpu::PSO* surface_giant = nullptr;
            gpu::PSO* surface_airless = nullptr;
            gpu::PSO* surface_rock = nullptr;
            gpu::PSO* cloud = nullptr;
            gpu::PSO* background = nullptr;
            gpu::PSO* galaxy = nullptr;
            gpu::PSO* atmosphere = nullptr;
            gpu::PSO* shadow = nullptr;
            gpu::PSO* motes = nullptr;
            gpu::PSO* stars = nullptr;
        } scene;
        struct {
            gpu::PSO* billboard = nullptr;
            gpu::PSO* cull = nullptr;
            gpu::PSO* splat = nullptr;
            gpu::PSO* blur = nullptr;
            gpu::PSO* splat_mask = nullptr;
            gpu::PSO* dust = nullptr;
            gpu::PSO* dust_blend = nullptr;
            gpu::PSO* disc = nullptr;
            gpu::PSO* disc_splat = nullptr;
        } belt;
        struct {
            gpu::PSO* bloom = nullptr;
            gpu::PSO* composite = nullptr;
            gpu::PSO* composite_hdr = nullptr; // the HDR variants target the 16-bit float intermediates
            gpu::PSO* sun_visibility = nullptr;
            gpu::PSO* flare = nullptr;
            gpu::PSO* present = nullptr;
            gpu::PSO* present_scrgb = nullptr; // the present and overlay pipelines follow the swapchain's format
            gpu::PSO* present_hdr10 = nullptr;
            gpu::PSO* meter = nullptr;
            gpu::PSO* temporal = nullptr;
            gpu::PSO* fxaa = nullptr;
            gpu::PSO* fxaa_hdr = nullptr;
            gpu::PSO* smaa_edges = nullptr;
            gpu::PSO* smaa_weights = nullptr;
            gpu::PSO* smaa_blend = nullptr;
            gpu::PSO* smaa_blend_hdr = nullptr;
        } post;
        gpu::PSO* ui = nullptr;
        gpu::PSO* ui_scrgb = nullptr;
        gpu::PSO* ui_hdr10 = nullptr;
    } pso;
    // The HDR output in effect, and the pipelines and formats that follow it.
    HdrOutput hdr_output = HdrOutput::Off;
    HdrOutput hdr_requested = HdrOutput::Off; // the last request, so an unsupported one is not re-queried every frame
    unsigned ui_display = 0;                  // the overlay's output encoding: mode in the low byte, paper white above
    bool hdr() const { return hdr_output != HdrOutput::Off; }
    gpu::Format intermediate_format() const { return hdr() ? gpu::Format::rgba16_float : gpu::Format::rgba8_srgb; }
    gpu::PSO* present_pso() const {
        return hdr_output == HdrOutput::ScRgb   ? pso.post.present_scrgb
               : hdr_output == HdrOutput::Hdr10 ? pso.post.present_hdr10
                                                : pso.post.present;
    }
    gpu::PSO* ui_pso() const {
        return hdr_output == HdrOutput::ScRgb ? pso.ui_scrgb : hdr_output == HdrOutput::Hdr10 ? pso.ui_hdr10 : pso.ui;
    }
    gpu::PSO* surface_pso(SurfaceKind kind) const {
        switch (kind) {
        case SurfaceKind::earth: return pso.scene.surface_earth;
        case SurfaceKind::giant: return pso.scene.surface_giant;
        case SurfaceKind::moon:
        case SurfaceKind::mars: return pso.scene.surface_airless;
        case SurfaceKind::rock: return pso.scene.surface_rock;
        }
        return pso.scene.surface_rock;
    }
    std::array<GpuMesh, geometry::lod_count> spheres{};
    std::array<GpuMesh, rock_group_count> rocks{}; // the rock library, indexed by rock_group; slices of rock_pool
    GpuMesh rock_pool{};                           // every rock mesh in one vertex and one index range
    std::array<GpuMesh, max_body_count> moonlet_meshes{}; // per body index; only moonlets are filled
    std::uint64_t rock_data = 0;                          // static heap address of the RockData records
    unsigned rock_count = 0;
    std::uint64_t rock_tail_data = 0;    // the size-tail rocks again, compacted for the transmittance splat
    std::vector<unsigned> rock_tail_ids; // their ids, ascending
    unsigned belt_count_override = 0;    // RendererConfig::belt_count
    SystemDescription system;
    std::filesystem::path directory;
    // The bodies occupy instance slots 0..body_count-1 in system order. The
    // three anchors carry atmospheres and shadow maps.
    Showcase showcase;
    unsigned body_count = 0;
    bool polar_caps = true;      // the gas giant's polar cap atlas loaded; the blend is skipped without it
    bool lens_dirt = true;       // the dirty-glass mask loaded; the effect stays off without it
    std::uint64_t star_data = 0; // static heap address of the Bright Star Catalogue records, 0 when absent
    unsigned star_count = 0;
    std::uint64_t splat_data = 0; // static heap address of the Milky Way's splat records, 0 when absent
    unsigned splat_count = 0;
    bool galaxy_layers_available = false;
    bool galaxy_original_available = false;

    // Frame state.
    Extent2D extent{};
    unsigned frame_index = 0;
    GpuTimings timings;
    Stats stats{};
    float adapted_exposure = 1, exposure_target = 1; // the filtered exposure and the meter's last target
    bool meter_pending = false;
    std::chrono::steady_clock::time_point meter_time{}; // last adaptation step
    FrameData previous_frame{};
    Vec3d previous_camera{};
    double previous_vertical_fov = 0;
    std::size_t previous_camera_cut = 0;
    bool history_valid = false;
    bool ui_overflow_logged = false;
    bool ui_font_uploaded = false;
    bool belt_disc_baked = false;          // the far-belt maps hold data (baked once the far tier is first needed)
    gpu::SwapchainInfo logged_swapchain{}; // last presentation mode and image count reported to the log

    // Per-frame scratch, cleared and reused: the body instances only, rocks
    // are placed by the GPU.
    std::vector<Instance> instances;

    ~Impl();

    // Device, shared heaps, uploads and target lifetime (renderer_resources.cpp).
    void init(void* window, const SystemDescription& description, const std::filesystem::path& base_directory,
              assets::ImageView hud, const RendererConfig& config);
    void create_device(void* window);
    void create_samplers();
    void create_fixed_targets();
    void resize(Extent2D new_extent, unsigned galaxy_divisor, unsigned flare_divisor);
    void set_hdr_output(HdrOutput mode);
    void resize_galaxy(unsigned divisor);
    void resize_flare(unsigned divisor);
    unsigned galaxy_divisor = 4, flare_divisor = 4;
    std::uint64_t upload_static(ByteView bytes);
    GpuImage create_image(const ImageDesc& desc);
    void bind(Slot slot, const GpuImage& image);
    void upload_images(std::span<const Upload> uploads);
    void upload_images(std::initializer_list<Upload> uploads) {
        upload_images(std::span<const Upload>(uploads.begin(), uploads.size()));
    }
    void upload_rgba(Slot slot, assets::ImageView pixels);

    // Pipeline creation and ownership registration (renderer_pipelines.cpp).
    void create_pipelines();
    gpu::PSO* create_pipeline(const PipelineDesc& desc);

    // Static mesh and material assets (renderer_assets.cpp).
    void create_meshes();
    void load_materials();
    void load_stars();
    void load_splats();
    void load_galaxy_layers(const assets::TextureSupport& support);
    void load_galaxy_original();
    GpuMesh upload_mesh(const geometry::Mesh& mesh);
    void upload_rock_pool(std::span<const geometry::Mesh> meshes);
    const GpuMesh& body_mesh(unsigned body, unsigned lod) const;

    // CPU frame packing (renderer_frame_data.cpp).
    FrameData build_frame(const FrameInput& input);
    void write_body_instances(const FrameInput& input, const FrameData& frame);
    void write_cull_scratch(const FrameInput& input, const FrameData& frame, CullScratch& scratch,
                            std::uint64_t instance_address);

    // Belt population, GPU culling, indirect batch and maps (renderer_belt.cpp).
    void build_belt(const BeltDescription& description);
    void read_cull_counts(const CullScratch& scratch);
    void record_cull_passes(gpu::CommandBuffer* cmd, const CullRoot& root);
    void record_belt_maps(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root, unsigned rock_limit,
                          bool light_map, float far_weight);
    void record_belt_light_pass(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root, unsigned rock_limit);
    void record_belt_disc_bakes(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root, unsigned rock_limit,
                                float far_weight);
    // Records draws inside the scene pass; other record_* methods own their render passes.
    void draw_rock_batch(gpu::CommandBuffer* cmd, Root& root, std::uint64_t args_address);
    void record_splat_mask_pass(gpu::CommandBuffer* cmd, Root root, std::uint64_t args_address);
    void record_belt_dust_passes(gpu::CommandBuffer* cmd, Root& root, bool enabled, float far_weight);

    // Bodies, sky and atmosphere (renderer_scene.cpp).
    void record_shadow_pass(gpu::CommandBuffer* cmd, Root root);
    void record_galaxy_pass(gpu::CommandBuffer* cmd, Root root, const FrameData& frame);
    void record_scene_pass(gpu::CommandBuffer* cmd, Root root, const FrameInput& input, const FrameData& frame,
                           std::uint64_t args_address);
    void record_atmosphere_passes(gpu::CommandBuffer* cmd, Root& root);
    void record_motion_streaks(gpu::CommandBuffer* cmd, Root& root);
    void draw_mesh(gpu::CommandBuffer* cmd, Root& root, const GpuMesh& mesh, unsigned base, unsigned instance_count);

    // Post-processing and exposure (renderer_post.cpp).
    void apply_metering(const ToneSettings& tone);
    void record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view, SpatialAA spatial_aa,
                            bool bloom, bool flare, bool motion_streaks, const ImDrawData* ui, std::uint8_t* ui_cpu,
                            std::uint64_t ui_gpu);
    void fullscreen_pass(gpu::CommandBuffer* cmd, GpuImage& target, gpu::PSO* pipeline, Root root,
                         bool preserve = false);

    // ImGui draws inside the presentation pass (renderer_overlay.cpp).
    void record_ui(gpu::CommandBuffer* cmd, const ImDrawData* ui, std::uint8_t* cpu, std::uint64_t gpu);

    // Frame orchestration and readback (renderer_frame.cpp).
    void read_gpu_timings();
};

} // namespace space::render
