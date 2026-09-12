#pragma once
// Private implementation shared by renderer_resources.cpp (device, heaps,
// meshes, materials, pipelines, targets) and renderer_frame.cpp (per-frame
// data, culling, pass recording, capture).
#include "render/renderer.hpp"

#include "assets/image.hpp"
#include "core/small_vec.hpp"
#include "core/types.hpp"
#include "render/gpu_types.hpp"
#include "scene/geometry.hpp"

#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace space::render {

// --- Fixed layout shared with the shaders -------------------------------------

// Sampled texture descriptor slots (shaders/common.slang binds 32).
enum class Slot : unsigned {
    hdr = 0,
    bloom_a = 1,
    bloom_b = 2,
    earth_albedo = 3,
    gas_albedo = 4,
    earth_clouds = 5,
    final_image = 6,
    earth_normal = 7,
    earth_specular = 8,
    earth_night = 9,
    moon_albedo = 10,
    rock_albedo = 11,
    rock_normal = 12,
    rock_roughness = 13,
    hud = 14,
    shadow_map = 15,
    history_a = 16,
    history_b = 17,
    depth = 18,
    mars_albedo = 19,
    mars_normal = 20,
    moon_normal = 21,
    rock_face_albedo = 22, // second and third rock sets follow the first's albedo, normal+height, roughness order
    rock_face_normal = 23,
    rock_face_roughness = 24,
    rock_boulder_albedo = 25,
    rock_boulder_normal = 26,
    rock_boulder_roughness = 27,
    count = 32,
};

// Sampler descriptor slots (shaders/common.slang binds 4).
enum class SamplerSlot : unsigned {
    clamp = 0,
    wrap_u_anisotropic = 1,
    shadow_compare = 2,
    wrap_anisotropic = 3,
    count = 4
};

// Root.mode as interpreted by surface.slang.
enum class SurfaceMode : std::uint32_t { opaque = 0, cloud = 1, shadow = 2, billboard = 3 };
// Root.mode as interpreted by post.slang; atmosphere.slang uses Root.base as the body index.
enum class PostMode : std::uint32_t { tonemap = 0, bloom_a = 1, bloom_b = 2, present = 3, meter = 4 };

// Instance.rotation_kind.w as interpreted by surface.slang and atmosphere.slang.
enum class SurfaceKind : unsigned { earth = 0, giant = 1, moon = 2, rock = 3, mars = 4 };

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

// --- Constants both files depend on ------------------------------------------

// One host-visible heap: meshes are appended from the front at startup, the
// per-frame FrameData and instance list live in the back half.
struct HeapLayout {
    std::uint64_t static_heap = 128ull << 20;   // with the 64 MiB upload staging this stays inside the BAR window
    std::uint64_t dynamic_offset = 80ull << 20; // meshes and rock records before, per-frame data after
    std::uint64_t cull_offset = 1024;           // FrameData, then the culling scratch, then the instances
    std::uint64_t instance_offset = 1024 + 8192;
    std::uint64_t staging_budget = 64ull << 20; // texture upload staging, see upload_images

    constexpr std::uint64_t instance_capacity() const {
        return (static_heap - dynamic_offset - instance_offset) / sizeof(Instance);
    }
};
inline constexpr HeapLayout heap_layout{};
static_assert(heap_layout.cull_offset >= sizeof(FrameData));
static_assert(heap_layout.instance_offset >= heap_layout.cull_offset + sizeof(CullScratch));
static_assert(heap_layout.dynamic_offset + heap_layout.instance_offset < heap_layout.static_heap);

// Sizes of the fixed GPU targets, created by the resources side and addressed by the frame side.
namespace targets {
inline constexpr Range<float> depth{0.02f, 2000.0f}; // near and far plane, camera-relative units
inline constexpr unsigned shadow_map_size = 2048;
inline constexpr unsigned meter_size = 16;     // luminance meter edge, texels of rgba32f
inline constexpr unsigned timestamp_count = 5; // frame start, after shadow, surface, atmosphere, post
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

// A sparse large-body tail exposes the fractured silhouettes between the much
// more numerous small rocks. Stable IDs keep quality tiers nested.
constexpr float size_tail(unsigned id) {
    return id % 137 == 0 ? 9.f : (id % 23 == 0 ? 4.f : 1.f);
}
} // namespace belt

// --- GPU-side records ---------------------------------------------------------

struct GpuImage {
    gpu::TextureHeap heap{};
    gpu::Texture* texture = nullptr;
    gpu::RenderView* view = nullptr;
};

struct GpuMesh {
    std::uint64_t vertices = 0, indices = 0; // GPU addresses in the static heap
    unsigned index_count = 0;
    unsigned first_index = 0, vertex_offset = 0; // position inside a pooled buffer, zero for standalone meshes
};

struct ImageDesc {
    Extent2D extent{1, 1};
    gpu::Format format = gpu::Format::rgba8_unorm;
    gpu::TextureUsage usage = gpu::TextureUsage::sampled;
    unsigned mips = 1;
};

enum class Blend { none, alpha, multiply }; // multiply: destination scaled by the fragment colour

struct PipelineDesc {
    const char* vertex_shader;   // shaders/<name>.vertex.spv
    const char* fragment_shader; // shaders/<name>.fragment.spv
    gpu::Format color_format;
    bool depth_test = false;
    Blend blend = Blend::none;
};

struct Upload {
    assets::MipChain mips;
    Slot slot;
};

// Uploads and their GPU images are created a handful at a time.
constexpr std::size_t inline_upload_count = 24;
using Uploads = SmallVec<Upload, inline_upload_count>;

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
    gpu::TimelineSemaphore* timeline = nullptr;
    std::uint64_t serial = 0;
    gpu::GpuHeap data{}, texture_descriptors{}, sampler_descriptors{}, luminance_readback{}, timestamps{};
    std::uint64_t static_cursor = 0;

    // Resources.
    std::vector<GpuImage> material_images;
    std::vector<gpu::PSO*> pipelines;
    GpuImage hdr{}, depth{}, bloom_a{}, bloom_b{}, final_image{}, shadow_map{}, luminance{}, history[2]{};
    struct {
        gpu::PSO *opaque = nullptr, *cloud = nullptr, *background = nullptr, *atmosphere = nullptr, *bloom = nullptr,
                 *post = nullptr, *present = nullptr, *shadow = nullptr, *meter = nullptr, *temporal = nullptr,
                 *cull = nullptr, *contact = nullptr;
    } pso;
    std::array<GpuMesh, geometry::lod_count> spheres{};
    std::array<GpuMesh, rock_group_count> rocks{}; // the rock library, indexed by rock_group; slices of rock_pool
    GpuMesh rock_pool{};                           // every rock mesh in one vertex and one index range
    std::array<GpuMesh, max_body_count> moonlet_meshes{}; // per body index; only moonlets are filled
    std::uint64_t rock_data = 0;                          // static heap address of the RockData records
    unsigned rock_count = 0;
    unsigned belt_count_override = 0; // RendererConfig::belt_count
    SystemDescription system;
    std::filesystem::path directory;
    // The bodies occupy instance slots 0..body_count-1 in system order. The
    // three anchors carry atmospheres and shadow maps.
    unsigned body_count = 0, earth_index = 0, giant_index = 0, mars_index = 0;

    // Frame state.
    Extent2D extent{};
    unsigned frame_index = 0;
    Stats stats{};
    float adapted_exposure = 1;
    bool meter_pending = false;
    std::chrono::steady_clock::time_point meter_time{}; // last adaptation step
    FrameData previous_frame{};
    Vec3d previous_camera{};
    bool history_valid = false;

    // Per-frame scratch, cleared and reused: the body instances only, rocks
    // are placed by the GPU.
    std::vector<Instance> instances;

    ~Impl();

    // renderer_resources.cpp
    void init(void* window, const SystemDescription& description, const std::filesystem::path& base_directory,
              const RendererConfig& config);
    void create_device(void* window);
    void create_samplers();
    void create_meshes();
    void build_belt(const BeltDescription& description);
    void load_materials();
    void create_pipelines();
    void create_fixed_targets();
    void resize(Extent2D new_extent);
    void destroy(GpuImage& image);
    std::uint64_t upload_static(ByteView bytes);
    GpuMesh upload_mesh(const geometry::Mesh& mesh);
    void upload_rock_pool(std::span<const geometry::Mesh> meshes);
    GpuImage create_image(const ImageDesc& desc);
    void bind(Slot slot, const GpuImage& image);
    const GpuMesh& body_mesh(unsigned body, unsigned lod) const;
    void upload_images(std::span<Upload> uploads);
    gpu::PSO* create_pipeline(const PipelineDesc& desc);

    // renderer_frame.cpp
    void read_gpu_timings();
    void apply_metering();
    FrameData build_frame(const FrameInput& input);
    void write_body_instances(const FrameInput& input, const FrameData& frame);
    void write_cull_scratch(const FrameInput& input, const FrameData& frame, CullScratch& scratch,
                            std::uint64_t instance_address);
    void read_cull_counts(const CullScratch& scratch);
    void record_cull_passes(gpu::CommandBuffer* cmd, const CullRoot& root);
    void record_shadow_pass(gpu::CommandBuffer* cmd, Root root);
    void record_scene_pass(gpu::CommandBuffer* cmd, Root root, const FrameInput& input, const FrameData& frame,
                           std::uint64_t args_address);
    void record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view);
    void fullscreen_pass(gpu::CommandBuffer* cmd, GpuImage& target, gpu::PSO* pipeline, Root root,
                         bool preserve = false);
    void draw_mesh(gpu::CommandBuffer* cmd, Root& root, const GpuMesh& mesh, unsigned base, unsigned instance_count);
};

} // namespace space::render
