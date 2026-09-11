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
#include <cstdint>
#include <span>
#include <vector>

namespace space::render {

// --- Fixed layout shared with the shaders -------------------------------------

// Sampled texture descriptor slots (shaders/common.slang binds 24).
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
    count = 24,
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

constexpr unsigned major_body_count = 3; // Earth, gas giant, moon: slots 0..2 of the instance list
constexpr unsigned earth_index = 0, giant_index = 1;

// --- Budgets and tuning -------------------------------------------------------

struct RenderSettings {
    struct {
        std::uint64_t static_heap = 48ull << 20;    // meshes, then the per-frame region
        std::uint64_t dynamic_offset = 32ull << 20; // per-frame data starts here
        std::uint64_t instance_offset = 512;        // instances follow the 400-byte FrameData
        std::uint64_t staging_budget = 64ull << 20; // texture upload staging, see upload_images
        unsigned decode_workers = 8;                // upper bound for the material decode pool
    } memory;
    struct {
        Range<float> depth{0.02f, 2000.0f}; // near and far plane, camera-relative units
        unsigned shadow_map_size = 2048;
        unsigned timestamp_count = 5; // frame start, after shadow, surface, atmosphere, post
    } frame;
    struct {
        unsigned target_size = 16; // luminance meter edge, texels of rgba32f
        unsigned interval = 16;    // frames between readbacks
        float min_weight = 0.004f; // texels below this weight do not vote
        float key = 0.18f;         // middle gray
        float rate = 0.08f;        // smoothing toward the target per readback
        Range<float> adapted{0.75f, 1.75f};
    } metering;
    struct {
        double max_jump = 10.0;       // camera translation that invalidates the history
        double min_forward_dot = 0.7; // turn that invalidates the history
    } history;
    struct {
        double giant_distance = 100.0; // inside this the shadow map follows the giant, else Earth
        float giant_half_size = 70.f, earth_half_size = 12.f;
    } shadow;
    struct {
        float disc_radius = 3.0f;       // Frame.sun.w
        float screen_size = 0.008f;     // Frame.screen_sun.w
        float max_screen_offset = 1.3f; // beyond this the flare is off-screen
    } sun;
    struct {
        unsigned high_count = 65000, baseline_count = 35000;
        unsigned angle_bins = 128, radial_bins = 8, height_bins = 4; // cluster grid
        float cluster_bound_scale = 1.30f;                           // the tilt/compression matrix has norm below 1.293
        float rock_radius_scale = 0.025f;                            // instance scale to world radius
        double spin_rate = 0.008, rock_spin_rate = 0.08;             // radians per simulation second
        float lod_switch_pixels = 5.f; // above this projected radius rocks use their own LOD
        struct {
            float min_pixels = 0.06f;  // smaller rocks are dropped
            float pixel_radius = 1.2f; // rocks below this become fixed-size billboards
        } billboard;
    } belt;
    float earth_rotation_offset = -0.95f; // aligns the day map with the lighting
};
inline constexpr RenderSettings settings{};

// --- GPU-side records ---------------------------------------------------------

struct GpuImage {
    gpu::TextureHeap heap{};
    gpu::Texture* texture = nullptr;
    gpu::RenderView* view = nullptr;
};

struct GpuMesh {
    std::uint64_t vertices = 0, indices = 0; // GPU addresses in the static heap
    unsigned index_count = 0;
};

struct ImageDesc {
    Extent2D extent{1, 1};
    gpu::Format format = gpu::Format::rgba8_unorm;
    gpu::TextureUsage usage = gpu::TextureUsage::sampled;
    unsigned mips = 1;
};

struct PipelineDesc {
    const char* vertex_shader;   // shaders/<name>.vertex.spv
    const char* fragment_shader; // shaders/<name>.fragment.spv
    gpu::Format color_format;
    bool depth_test = false;
    bool alpha_blend = false;
};

struct Upload {
    assets::MipChain mips;
    Slot slot;
};

// Uploads and their GPU images are created a handful at a time.
constexpr std::size_t inline_upload_count = 16;
using Uploads = SmallVec<Upload, inline_upload_count>;

// Spatial bin of belt instances for coarse frustum and occlusion culling.
// indices views Impl::belt_order, which is immutable after build_belt.
struct BeltCluster {
    Vec3f center{};
    float radius = 0;
    std::span<const unsigned> indices;
};

// Where each LOD group and the billboards landed in the per-frame instance list.
struct BeltBatches {
    std::array<unsigned, geometry::lod_count> bases{}, counts{};
    unsigned distant_base = 0, distant_count = 0;
};

struct RockCullContext;

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
                 *post = nullptr, *present = nullptr, *shadow = nullptr, *meter = nullptr, *temporal = nullptr;
    } pso;
    std::array<GpuMesh, geometry::lod_count> spheres{}, rocks{};
    std::vector<geometry::AsteroidInstance> belt;
    std::vector<unsigned> belt_order; // belt indices grouped by cluster
    std::vector<BeltCluster> belt_clusters;
    SystemDescription system;
    std::filesystem::path directory;

    // Frame state.
    Extent2D extent{};
    unsigned frame_index = 0;
    Stats stats{};
    float adapted_exposure = 1;
    bool meter_pending = false;
    FrameData previous_frame{};
    Vec3d previous_camera{};
    bool history_valid = false;

    // Per-frame scratch, cleared and reused.
    std::vector<Instance> instances;
    std::array<std::vector<Instance>, geometry::lod_count> lod_groups;
    std::vector<Instance> distant;

    ~Impl();

    // renderer_resources.cpp
    void init(void* window, const SystemDescription& description, const std::filesystem::path& base_directory);
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
    GpuImage create_image(const ImageDesc& desc);
    void bind(Slot slot, const GpuImage& image);
    void upload_images(std::span<Upload> uploads);
    gpu::PSO* create_pipeline(const PipelineDesc& desc);

    // renderer_frame.cpp
    void read_gpu_timings();
    void apply_metering();
    FrameData build_frame(const FrameInput& input);
    BeltBatches cull_belt(const FrameInput& input, const FrameData& frame);
    void classify_rock(unsigned id, const RockCullContext& context);
    void record_shadow_pass(gpu::CommandBuffer* cmd, Root root, const BeltBatches& batches);
    void record_scene_pass(gpu::CommandBuffer* cmd, Root root, const FrameInput& input, const FrameData& frame,
                           const BeltBatches& batches);
    void record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view);
    void fullscreen_pass(gpu::CommandBuffer* cmd, GpuImage& target, gpu::PSO* pipeline, Root root,
                         bool preserve = false);
    void draw_mesh(gpu::CommandBuffer* cmd, Root& root, const GpuMesh& mesh, unsigned base, unsigned instance_count);
};

} // namespace space::render
