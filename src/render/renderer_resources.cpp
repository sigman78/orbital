#include "render/renderer_impl.hpp"

#include "app/hud.hpp"
#include "assets/kernels.hpp"
#include "assets/materials.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include "core/panic.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <future>
#include <iterator>
#include <thread>

namespace space::render {
namespace {

constexpr unsigned angle_bins = 128, radial_bins = 8, height_bins = 4;
constexpr unsigned cluster_bin_count = angle_bins * radial_bins * height_bins;
constexpr unsigned max_decode_workers = 8;
constexpr float rock_radius_scale = 0.025f; // instance scale to world radius

// A sparse large-body tail exposes the fractured silhouettes between the much
// more numerous small rocks. Stable IDs keep quality tiers nested.
float size_tail(unsigned id) {
    return id % 137 == 0 ? 9.f : (id % 23 == 0 ? 4.f : 1.f);
}

std::vector<std::uint32_t> read_spirv(const std::filesystem::path& path) {
    const auto bytes = file::read(path);
    if (!bytes || bytes->empty() || bytes->size() % 4)
        panic(std::format("missing or invalid shader: {}", path.string()));
    std::vector<std::uint32_t> words(bytes->size() / 4);
    std::memcpy(words.data(), bytes->data(), bytes->size());
    constexpr std::uint32_t spirv_magic = 0x07230203;
    if (words[0] != spirv_magic)
        panic(std::format("not a SPIR-V module: {}", path.string()));
    return words;
}

std::uint64_t padded_size(const assets::Image& mip) {
    return (mip.pixels.size() + 15) & ~15ull;
}

struct MaterialSource {
    const char* file;
    Slot slot;
    assets::MaterialDesc desc;
};

using assets::MaterialEncoding;
constexpr MaterialSource material_sources[] = {
    {"earth_albedo.png", Slot::earth_albedo, {.encoding = MaterialEncoding::SRGB}},
    {"gas_albedo.png", Slot::gas_albedo, {.encoding = MaterialEncoding::SRGB}},
    {"earth_clouds.png", Slot::earth_clouds, {.luminance_to_alpha = true}},
    {"earth_normal.png", Slot::earth_normal, {.normal_map = true}},
    {"earth_specular.png", Slot::earth_specular, {}},
    {"earth_night.png", Slot::earth_night, {.encoding = MaterialEncoding::SRGB}},
    {"moon_albedo.png", Slot::moon_albedo, {.encoding = MaterialEncoding::SRGB}},
    {"rock_albedo.png", Slot::rock_albedo, {.encoding = MaterialEncoding::SRGB}},
    {"rock_normal.png", Slot::rock_normal, {.normal_map = true}},
    {"rock_roughness.png", Slot::rock_roughness, {}},
};
constexpr std::size_t material_count = std::size(material_sources);

} // namespace

Renderer::Impl::~Impl() {
    if (device)
        gpu::wait_idle(device);
    for (auto* pipeline : pipelines)
        gpu::destroy_pso(pipeline);
    for (auto& image : material_images)
        destroy(image);
    for (auto* image :
         {&hdr, &depth, &bloom_a, &bloom_b, &final_image, &shadow_map, &luminance, &history[0], &history[1]})
        destroy(*image);
    for (auto* heap : {&data, &texture_descriptors, &sampler_descriptors, &luminance_readback, &timestamps})
        gpu::destroy_gpu_heap(*heap);
    gpu::destroy_timeline_semaphore(timeline);
    gpu::destroy_device(device);
}

void Renderer::Impl::destroy(GpuImage& image) {
    gpu::destroy_render_view(image.view);
    gpu::destroy_texture(image.texture);
    gpu::destroy_texture_heap(image.heap);
    image = {};
}

std::uint64_t Renderer::Impl::upload_static(const void* bytes, std::size_t size) {
    static_cursor = (static_cursor + 15) & ~15ull;
    if (static_cursor + size > dynamic_offset)
        panic("static GPU heap exhausted");
    const auto address = reinterpret_cast<std::uint64_t>(data.range.gpu) + static_cursor;
    std::memcpy(data.range.cpu + static_cursor, bytes, size);
    static_cursor += size;
    return address;
}

GpuMesh Renderer::Impl::upload_mesh(const geometry::Mesh& mesh) {
    std::vector<Vertex> vertices;
    vertices.reserve(mesh.vertices.size());
    for (const auto& v : mesh.vertices)
        vertices.push_back({{v.position.x, v.position.y, v.position.z, 1}, {v.normal.x, v.normal.y, v.normal.z, 0}});
    return {.vertices = upload_static(vertices.data(), vertices.size() * sizeof(Vertex)),
            .indices = upload_static(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t)),
            .index_count = unsigned(mesh.indices.size())};
}

GpuImage Renderer::Impl::create_image(const ImageDesc& desc) {
    const gpu::TextureDesc texture_desc{
        .extent = {desc.width, desc.height, 1}, .mip_levels = desc.mips, .format = desc.format, .usage = desc.usage};
    const auto size = gpu::get_texture_size_align(device, texture_desc);
    GpuImage result;
    result.heap = gpu::create_texture_heap(device, size.size);
    result.texture = gpu::create_texture(device, texture_desc, result.heap, 0);
    if (!result.texture)
        panic(std::format("texture allocation failed ({}x{}, {} mips)", desc.width, desc.height, desc.mips));
    const auto attachment_bits = static_cast<unsigned>(gpu::TextureUsage::color_attachment |
                                                       gpu::TextureUsage::depth_stencil_attachment);
    if (static_cast<unsigned>(desc.usage) & attachment_bits)
        result.view = gpu::create_render_view(result.texture);
    return result;
}

void Renderer::Impl::bind(Slot slot, const GpuImage& image) {
    ORBITAL_ASSERT(slot < Slot::count);
    const auto& caps = gpu::get_device_caps(device);
    gpu::write_texture_descriptor(device, texture_descriptors.range.cpu + unsigned(slot) * caps.texture_descriptor_size,
                                  image.texture, gpu::TextureDescriptorType::sampled);
}

// Creates one sampled RGBA8 texture per upload and streams every mip through
// a single reusable staging heap. Host-visible heaps live in device-local
// (BAR) memory on this backend, so the heap is capped and flushed in batches
// instead of sized to the whole set. Textures are created before
// begin_commands so the backend records their layout initialization first.
void Renderer::Impl::upload_images(std::span<Upload> uploads) {
    std::vector<GpuImage> images;
    std::uint64_t largest = 0;
    for (const auto& upload : uploads) {
        ORBITAL_ASSERT(!upload.mips.empty());
        const auto& base = upload.mips.front();
        images.push_back(create_image({.width = base.width,
                                       .height = base.height,
                                       .format = gpu::Format::rgba8_unorm,
                                       .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::transfer_destination,
                                       .mips = unsigned(upload.mips.size())}));
        material_images.push_back(images.back());
        bind(upload.slot, images.back());
        std::uint64_t bytes = 0;
        for (const auto& mip : upload.mips)
            bytes += padded_size(mip);
        largest = std::max(largest, bytes);
    }
    auto staging = gpu::create_gpu_heap(device, std::max(largest, staging_budget));
    if (!staging.range.cpu)
        panic("texture staging allocation failed");
    gpu::CommandBuffer* cmd = nullptr;
    std::uint64_t offset = 0;
    const auto flush = [&] {
        if (!cmd)
            return;
        gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::fragment,
                     gpu::Access::shader_read);
        gpu::submit({cmd}, {timeline, ++serial});
        gpu::wait_timeline({timeline, serial});
        cmd = nullptr;
        offset = 0;
    };
    for (std::size_t i = 0; i < uploads.size(); i++) {
        std::uint64_t bytes = 0;
        for (const auto& mip : uploads[i].mips)
            bytes += padded_size(mip);
        if (offset + bytes > staging.range.size)
            flush();
        if (!cmd)
            cmd = gpu::begin_commands(device);
        for (unsigned level = 0; level < uploads[i].mips.size(); level++) {
            const auto& mip = uploads[i].mips[level];
            std::memcpy(staging.range.cpu + offset, mip.pixels.data(), mip.pixels.size());
            const auto source = reinterpret_cast<std::uint64_t>(staging.range.gpu) + offset;
            gpu::copy_memory_to_texture(cmd, {reinterpret_cast<void*>(source), mip.pixels.size()}, images[i].texture,
                                        {.mip_level = level});
            offset += padded_size(mip);
        }
    }
    flush();
    gpu::destroy_gpu_heap(staging);
}

gpu::PSO* Renderer::Impl::create_pipeline(const PipelineDesc& desc) {
    const auto vertex = read_spirv(directory / "shaders" / std::format("{}.vertex.spv", desc.vertex_shader));
    const auto fragment = read_spirv(directory / "shaders" / std::format("{}.fragment.spv", desc.fragment_shader));
    gpu::BlendState blending{};
    if (desc.alpha_blend)
        blending = {.enabled = true,
                    .color = {gpu::BlendFactor::source_alpha, gpu::BlendFactor::one_minus_source_alpha},
                    .alpha = {gpu::BlendFactor::one, gpu::BlendFactor::one_minus_source_alpha}};
    const gpu::ColorTargetDesc target{.format = desc.color_format, .blend = blending};
    auto* pipeline = gpu::create_graphics_pso(
        device, {.vertex_spirv = vertex,
                 .fragment_spirv = fragment,
                 .color_targets = {&target, 1},
                 .depth_format = desc.depth_test ? gpu::Format::d32_float : gpu::Format::undefined});
    if (!pipeline)
        panic(std::format("pipeline creation failed for {} + {}", desc.vertex_shader, desc.fragment_shader));
    pipelines.push_back(pipeline);
    return pipeline;
}

void Renderer::Impl::create_device(void* window) {
    const auto init = gpu::create_device(
        {.window = window, .swapchain_format = gpu::Format::bgra8_srgb, .timestamp_query_count = 16});
    device = init.device;
    if (!device)
        panic("Vulkan device creation failed; check the console for missing features or driver errors");
    const auto& caps = gpu::get_device_caps(device);
    log::info("GPU: {} | conventional NoGraphicsAPI backend", caps.device_name);
    if (!caps.conventional_descriptor_backend)
        panic("the demo's shaders require the conventional descriptor backend build option");
    timeline = gpu::create_timeline_semaphore(device);
    data = gpu::create_gpu_heap(device, static_heap_size);
    texture_descriptors = gpu::create_gpu_heap(device, caps.texture_descriptor_size * unsigned(Slot::count),
                                               gpu::MemoryType::texture_descriptor_heap);
    sampler_descriptors = gpu::create_gpu_heap(device, caps.sampler_descriptor_size * unsigned(SamplerSlot::count),
                                               gpu::MemoryType::sampler_descriptor_heap);
    if (!data.range.cpu || !texture_descriptors.range.cpu || !sampler_descriptors.range.cpu)
        panic("GPU mapped heap allocation failed");
    luminance_readback = gpu::create_gpu_heap(device, luminance_size * luminance_size * sizeof(Float4),
                                              gpu::MemoryType::readback);
    timestamps = gpu::create_gpu_heap(device, 64, gpu::MemoryType::readback);
}

void Renderer::Impl::create_samplers() {
    const auto& caps = gpu::get_device_caps(device);
    const auto write = [&](SamplerSlot slot, const gpu::SamplerDesc& desc) {
        gpu::write_sampler_descriptor(
            device, sampler_descriptors.range.cpu + unsigned(slot) * caps.sampler_descriptor_size, desc);
    };
    using gpu::AddressMode;
    write(SamplerSlot::clamp, {.address_u = AddressMode::clamp_to_edge,
                               .address_v = AddressMode::clamp_to_edge,
                               .address_w = AddressMode::clamp_to_edge});
    write(SamplerSlot::wrap_u_anisotropic, {.address_u = AddressMode::repeat,
                                            .address_v = AddressMode::clamp_to_edge,
                                            .address_w = AddressMode::clamp_to_edge,
                                            .anisotropic = true});
    write(SamplerSlot::shadow_compare, {.address_u = AddressMode::clamp_to_edge,
                                        .address_v = AddressMode::clamp_to_edge,
                                        .address_w = AddressMode::clamp_to_edge,
                                        .compare_enabled = true});
    write(SamplerSlot::wrap_anisotropic, {.address_u = AddressMode::repeat,
                                          .address_v = AddressMode::repeat,
                                          .address_w = AddressMode::clamp_to_edge,
                                          .anisotropic = true});
}

void Renderer::Impl::create_meshes() {
    for (unsigned lod = 0; lod < geometry::lod_count; lod++) {
        spheres[lod] = upload_mesh(geometry::generate_sphere(32u << lod, 16u << lod));
        rocks[lod] = upload_mesh(geometry::generate_rock(71 + lod * 37, 2 + lod));
    }
}

// The seeded belt order defines quality tiers, but is spatially random. Build
// a separate index into compact polar cells without reordering IDs.
void Renderer::Impl::build_belt(const BeltDescription& description) {
    const float inner = float(description.inner_radius), outer = float(description.outer_radius),
                thickness = float(description.thickness);
    belt = geometry::generate_belt({.seed = description.seed,
                                    .count = high_belt_count,
                                    .inner_radius = inner,
                                    .outer_radius = outer,
                                    .thickness = thickness});
    std::vector<std::vector<unsigned>> bins(cluster_bin_count);
    for (unsigned i = 0; i < belt.size(); ++i) {
        const auto& p = belt[i].position;
        float angle = std::atan2(p.z, p.x);
        if (angle < 0)
            angle += 2 * pi<float>;
        const float radial = std::sqrt(p.x * p.x + p.z * p.z);
        const unsigned a = std::min(angle_bins - 1, unsigned(angle / (2 * pi<float>)*angle_bins));
        const unsigned r = std::min(
            radial_bins - 1,
            unsigned(std::clamp((radial - inner) / std::max(outer - inner, 1e-4f), 0.f, .999999f) * radial_bins));
        const unsigned h = std::min(
            height_bins - 1,
            unsigned(std::clamp((p.y + thickness * .5f) / std::max(thickness, 1e-4f), 0.f, .999999f) * height_bins));
        bins[(h * radial_bins + r) * angle_bins + a].push_back(i);
    }
    belt_clusters.reserve(cluster_bin_count);
    for (auto& ids : bins) {
        if (ids.empty())
            continue;
        Vec3f lo = belt[ids[0]].position, hi = lo;
        for (unsigned id : ids) {
            const auto& p = belt[id].position;
            lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
        const Vec3f center = (lo + hi) * .5f;
        float bound = 0;
        for (unsigned id : ids) {
            const auto d = belt[id].position - center;
            const auto& s = belt[id].scale;
            const float rock_radius = rock_radius_scale * std::max(s.x, std::max(s.y, s.z)) * size_tail(id);
            bound = std::max(bound, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) + rock_radius);
        }
        belt_clusters.push_back({.center = center, .radius = bound, .indices = std::move(ids)});
    }
}

// Decodes and mip-filters every material on a bounded worker pool, then
// uploads them all in one batch on this thread. Unused descriptor slots point
// at the first map so every binding is valid.
void Renderer::Impl::load_materials() {
    const auto start = std::chrono::steady_clock::now();
    // Leave one core for this thread and cap the pool: decoding and filtering
    // are memory-bound enough that more workers stop helping.
    const unsigned cores = std::thread::hardware_concurrency();
    const std::size_t workers = std::min(material_count,
                                         std::size_t(std::clamp(cores > 1 ? cores - 1 : 1u, 1u, max_decode_workers)));
    std::vector<Upload> uploads(material_count);
    std::atomic<std::size_t> next{0};
    std::vector<std::future<void>> pool;
    for (std::size_t worker = 0; worker < workers; worker++)
        pool.push_back(std::async(std::launch::async, [&] {
            for (std::size_t i = next++; i < material_count; i = next++) {
                const auto& source = material_sources[i];
                uploads[i] = {.mips = assets::load_material(directory / "assets/materials" / source.file, source.desc),
                              .slot = source.slot};
            }
        }));
    for (auto& worker : pool)
        worker.get();
    const auto first_material = material_images.size();
    upload_images(uploads);
    bool used[unsigned(Slot::count)]{};
    for (const auto& source : material_sources)
        used[unsigned(source.slot)] = true;
    for (unsigned slot = 0; slot < unsigned(Slot::count); slot++)
        if (!used[slot])
            bind(Slot(slot), material_images[first_material]);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                               start);
    log::info("Loaded {} materials in {} ms on {} workers ({})", material_count, elapsed.count(), workers,
              assets::kernels::backend());
}

void Renderer::Impl::create_pipelines() {
    using gpu::Format;
    const auto make = [&](const char* vertex, const char* fragment, Format format, bool depth_test = false,
                          bool alpha_blend = false) {
        return create_pipeline({.vertex_shader = vertex,
                                .fragment_shader = fragment,
                                .color_format = format,
                                .depth_test = depth_test,
                                .alpha_blend = alpha_blend});
    };
    pso.opaque = make("surface", "surface", Format::rgba16_float, true);
    pso.cloud = make("surface", "surface", Format::rgba16_float, true, true);
    pso.background = make("fullscreen", "background", Format::rgba16_float, true);
    pso.atmosphere = make("fullscreen", "atmosphere", Format::rgba16_float, false, true);
    pso.bloom = make("fullscreen", "post", Format::rgba16_float);
    pso.post = make("fullscreen", "post", Format::rgba8_srgb);
    pso.present = make("fullscreen", "post", Format::bgra8_srgb);
    pso.meter = make("fullscreen", "post", Format::rgba32_float);
    pso.temporal = make("fullscreen", "temporal", Format::rgba16_float);
    // Depth-only shadow pass reuses the surface vertex shader with a slope bias.
    const auto shadow_vertex = read_spirv(directory / "shaders/surface.vertex.spv");
    pso.shadow = gpu::create_graphics_pso(device,
                                          {.vertex_spirv = shadow_vertex,
                                           .depth_format = Format::d32_float,
                                           .rasterization = {.depth_bias_constant = 1, .depth_bias_slope = 1.5f}});
    if (!pso.shadow)
        panic("shadow pipeline creation failed");
    pipelines.push_back(pso.shadow);
}

void Renderer::Impl::create_fixed_targets() {
    shadow_map = create_image({.width = shadow_map_size,
                               .height = shadow_map_size,
                               .format = gpu::Format::d32_float,
                               .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::depth_stencil_attachment});
    bind(Slot::shadow_map, shadow_map);
    luminance = create_image({.width = luminance_size,
                              .height = luminance_size,
                              .format = gpu::Format::rgba32_float,
                              .usage = gpu::TextureUsage::color_attachment | gpu::TextureUsage::transfer_source});
}

void Renderer::Impl::init(void* window, const SystemDescription& description,
                          const std::filesystem::path& base_directory) {
    system = description;
    directory = base_directory;
    ORBITAL_ASSERT(system.bodies.size() >= major_body_count && !system.belts.empty());
    create_device(window);
    create_samplers();
    create_meshes();
    build_belt(system.belts[0]);
    log::info("Loading planetary maps and scanned rock PBR materials...");
    load_materials();
    Upload hud[] = {{.mips = {assets::make_hud()}, .slot = Slot::hud}};
    upload_images(hud);
    create_pipelines();
    create_fixed_targets();
}

void Renderer::Impl::resize(unsigned new_width, unsigned new_height) {
    if (width == new_width && height == new_height)
        return;
    gpu::wait_idle(device);
    for (auto* image : {&hdr, &depth, &bloom_a, &bloom_b, &final_image, &history[0], &history[1]})
        destroy(*image);
    width = new_width;
    height = new_height;
    history_valid = false;
    const auto color_usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment;
    const unsigned bloom_width = std::max(1u, width / 4), bloom_height = std::max(1u, height / 4);
    hdr = create_image({.width = width, .height = height, .format = gpu::Format::rgba16_float, .usage = color_usage});
    depth = create_image({.width = width,
                          .height = height,
                          .format = gpu::Format::d32_float,
                          .usage = gpu::TextureUsage::depth_stencil_attachment | gpu::TextureUsage::sampled});
    bloom_a = create_image(
        {.width = bloom_width, .height = bloom_height, .format = gpu::Format::rgba16_float, .usage = color_usage});
    bloom_b = create_image(
        {.width = bloom_width, .height = bloom_height, .format = gpu::Format::rgba16_float, .usage = color_usage});
    final_image = create_image({.width = width,
                                .height = height,
                                .format = gpu::Format::rgba8_srgb,
                                .usage = color_usage | gpu::TextureUsage::transfer_source});
    for (auto& image : history)
        image = create_image(
            {.width = width, .height = height, .format = gpu::Format::rgba16_float, .usage = color_usage});
    bind(Slot::hdr, hdr);
    bind(Slot::bloom_a, bloom_a);
    bind(Slot::bloom_b, bloom_b);
    bind(Slot::final_image, final_image);
    bind(Slot::depth, depth);
}

Renderer::Renderer(void* window, const SystemDescription& system, const std::filesystem::path& directory)
    : impl_(std::make_unique<Impl>()) {
    impl_->init(window, system, directory);
}

Renderer::~Renderer() = default;

Stats Renderer::stats() const {
    return impl_->stats;
}

} // namespace space::render
