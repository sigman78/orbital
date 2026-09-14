#include "render/renderer_impl.hpp"

#include "assets/smaa.hpp"
#include "core/log.hpp"
#include "core/panic_if.hpp"
#include <algorithm>
#include <cstring>
#include <format>

namespace space::render {

namespace {
std::uint64_t padded_size(const assets::TextureMip& mip) {
    return (mip.bytes.size() + 15) & ~15ull;
}

void flush_upload(gpu::CommandBuffer*& cmd, std::uint64_t& offset, gpu::SubmissionTimeline& submissions) {
    if (!cmd)
        return;
    synchronize(cmd, access::transfer_write, access::fragment_sample);
    submissions.submit_and_wait({cmd});
    cmd = nullptr;
    offset = 0;
}

} // namespace

Renderer::Impl::~Impl() {
    if (device)
        gpu::wait_idle(device);
    pipelines.clear();
    material_images.clear();
    frame_targets = {};
    fixed_targets = {};
    buffers = {};
    submissions.reset();
    gpu::destroy_device(device);
}

std::uint64_t Renderer::Impl::upload_static(ByteView bytes) {
    static_cursor = (static_cursor + 15) & ~15ull;
    panic_if(static_cursor + bytes.size() > heap_layout.dynamic_offset, "static GPU heap exhausted");
    const auto address = reinterpret_cast<std::uint64_t>(buffers.data.range().gpu) + static_cursor;
    std::memcpy(buffers.data.range().cpu + static_cursor, bytes.data(), bytes.size());
    static_cursor += bytes.size();
    return address;
}

GpuImage Renderer::Impl::create_image(const ImageDesc& desc) {
    return GpuImage::create(device, desc);
}

void Renderer::Impl::bind(Slot slot, const GpuImage& image) {
    ORBITAL_ASSERT(slot < Slot::count);
    const auto& caps = gpu::get_device_caps(device);
    gpu::write_texture_descriptor(
        device, buffers.texture_descriptors.range().cpu + unsigned(slot) * caps.texture_descriptor_size,
        image.texture(), gpu::TextureDescriptorType::sampled);
}

// Creates one sampled texture per upload and streams every mip through
// a single reusable staging heap. Host-visible heaps live in device-local
// (BAR) memory on this backend, so the heap is capped and flushed in batches
// instead of sized to the whole set. Textures are created before
// begin_commands so the backend records their layout initialization first.
void Renderer::Impl::upload_images(std::span<const Upload> uploads) {
    const auto first_image = material_images.size();
    material_images.reserve(first_image + uploads.size());
    std::uint64_t largest = 0;
    for (const auto& upload : uploads) {
        ORBITAL_ASSERT(assets::valid_texture(upload.data));
        const auto& base = upload.data.mips.front();
        material_images.push_back(
            create_image({.extent = base.extent,
                          .format = texture_format(upload.data),
                          .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::transfer_destination,
                          .mips = unsigned(upload.data.mips.size())}));
        bind(upload.slot, material_images.back());
        std::uint64_t bytes = 0;
        for (const auto& mip : upload.data.mips)
            bytes += padded_size(mip);
        largest = std::max(largest, bytes);
    }
    auto staging = gpu::UniqueGpuHeap::create(device, std::max(largest, heap_layout.staging_budget));
    panic_if(!staging.range().cpu, "texture staging allocation failed");
    gpu::CommandBuffer* cmd = nullptr;
    std::uint64_t offset = 0;
    for (std::size_t i = 0; i < uploads.size(); i++) {
        std::uint64_t bytes = 0;
        for (const auto& mip : uploads[i].data.mips)
            bytes += padded_size(mip);
        if (offset + bytes > staging.range().size)
            flush_upload(cmd, offset, submissions);
        if (!cmd)
            cmd = gpu::begin_commands(device);
        for (unsigned level = 0; level < uploads[i].data.mips.size(); level++) {
            const auto& mip = uploads[i].data.mips[level];
            std::memcpy(staging.range().cpu + offset, mip.bytes.data(), mip.bytes.size());
            const auto source = reinterpret_cast<std::uint64_t>(staging.range().gpu) + offset;
            gpu::copy_memory_to_texture(cmd, {reinterpret_cast<void*>(source), mip.bytes.size()},
                                        material_images[first_image + i].texture(), {.mip_level = level});
            offset += padded_size(mip);
        }
    }
    flush_upload(cmd, offset, submissions);
    staging.reset();
}

void Renderer::Impl::upload_rgba(Slot slot, assets::ImageView pixels) {
    ORBITAL_ASSERT(pixels.layout() == assets::PixelLayout::Rgba8);
    assets::Rgba8Image image{.extent = pixels.extent(), .pixels = {}};
    image.pixels.reserve(image.pixel_count() * 4);
    for (unsigned y = 0; y < image.extent.height; ++y) {
        const auto row = pixels.row(y);
        image.pixels.insert(image.pixels.end(), row.begin(), row.end());
    }
    upload_images({{.data = assets::texture_from_image(std::move(image)), .slot = slot}});
}

void Renderer::Impl::create_device(void* window) {
    const auto init = gpu::create_device(
        {.window = window, .swapchain_format = gpu::Format::bgra8_srgb, .timestamp_query_count = 16});
    device = init.device;
    panic_if(!device, "Vulkan device creation failed; check the console for missing features or driver errors");
    const auto& caps = gpu::get_device_caps(device);
    log::info("GPU: {} | conventional NoGraphicsAPI backend", caps.device_name);
    panic_if(!caps.conventional_descriptor_backend,
             "the demo's shaders require the conventional descriptor backend build option");
    submissions.initialize(device);
    buffers.data = gpu::UniqueGpuHeap::create(device, heap_layout.mapped_size());
    buffers.cull_device = gpu::UniqueGpuHeap::create(
        device, heap_layout.cull_size(std::max(high_quality.belt_count, belt_count_override), body_count),
        gpu::MemoryType::gpu_only);
    buffers.cull_readback = gpu::UniqueGpuHeap::create(device, sizeof(CullScratch), gpu::MemoryType::readback);
    panic_if(!buffers.cull_device.range().gpu || !buffers.cull_readback.range().cpu, "culling heap allocation failed");
    log::info("Buffer heaps: {} KiB mapped, {} KiB device-only culling", buffers.data.range().size / 1024,
              buffers.cull_device.range().size / 1024);
    buffers.texture_descriptors = gpu::UniqueGpuHeap::create(
        device, caps.texture_descriptor_size * unsigned(Slot::count), gpu::MemoryType::texture_descriptor_heap);
    buffers.sampler_descriptors = gpu::UniqueGpuHeap::create(
        device, caps.sampler_descriptor_size * unsigned(SamplerSlot::count), gpu::MemoryType::sampler_descriptor_heap);
    panic_if(!buffers.data.range().cpu || !buffers.texture_descriptors.range().cpu ||
                 !buffers.sampler_descriptors.range().cpu,
             "GPU mapped heap allocation failed");
    buffers.luminance_readback = gpu::UniqueGpuHeap::create(
        device, targets::meter_size * targets::meter_size * sizeof(Float4), gpu::MemoryType::readback);
    buffers.timestamps = gpu::UniqueGpuHeap::create(device, targets::timestamp_count * sizeof(std::uint64_t),
                                                    gpu::MemoryType::readback);
}

void Renderer::Impl::create_samplers() {
    const auto& caps = gpu::get_device_caps(device);
    const auto write = [&](SamplerSlot slot, const gpu::SamplerDesc& desc) {
        gpu::write_sampler_descriptor(
            device, buffers.sampler_descriptors.range().cpu + unsigned(slot) * caps.sampler_descriptor_size, desc);
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

void Renderer::Impl::create_fixed_targets() {
    fixed_targets.shadow_map = create_image(
        {.extent = {targets::shadow_map_size, targets::shadow_map_size},
         .format = gpu::Format::d32_float,
         .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::depth_stencil_attachment});
    bind(Slot::shadow_map, fixed_targets.shadow_map);
    for (auto* map : {&fixed_targets.belt_light, &fixed_targets.belt_light_blur})
        *map = create_image({.extent = {targets::belt_light_map_size, targets::belt_light_map_size},
                             .format = gpu::Format::rgba16_float,
                             .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    bind(Slot::belt_light_map, fixed_targets.belt_light);
    bind(Slot::belt_light_blur, fixed_targets.belt_light_blur);
    fixed_targets.belt_disc_light = create_image(
        {.extent = {targets::belt_disc_map_size, targets::belt_disc_map_size},
         .format = gpu::Format::rgba16_float,
         .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    fixed_targets.belt_disc_rocks = create_image(
        {.extent = {targets::belt_disc_rock_map_size, targets::belt_disc_rock_map_size},
         .format = gpu::Format::rgba16_float,
         .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    bind(Slot::belt_disc_light, fixed_targets.belt_disc_light);
    bind(Slot::belt_disc_rocks, fixed_targets.belt_disc_rocks);
    fixed_targets.luminance = create_image(
        {.extent = {targets::meter_size, targets::meter_size},
         .format = gpu::Format::rgba32_float,
         .usage = gpu::TextureUsage::color_attachment | gpu::TextureUsage::transfer_source});
}

void Renderer::Impl::init(void* window, const SystemDescription& description,
                          const std::filesystem::path& base_directory, assets::ImageView hud,
                          const RendererConfig& config) {
    system = description;
    directory = base_directory;
    belt_count_override = config.belt_count;
    std::string scene_error;
    const auto resolved = Showcase::resolve(system, scene_error);
    panic_if(!resolved, "unsupported renderer scene: {}", scene_error);
    showcase = *resolved;
    body_count = showcase.body_count();
    panic_if(std::max(high_quality.belt_count, belt_count_override) > heap_layout.instance_capacity() - body_count,
             "belt count exceeds culling capacity (maximum {} rocks)", heap_layout.instance_capacity() - body_count);
    create_device(window);
    create_samplers();
    create_meshes();
    build_belt(system.belts.front());
    log::info("Loading planetary maps and scanned rock PBR materials...");
    load_materials();
    upload_rgba(Slot::hud, hud);
    // Embedded SMAA lookup tables, widened to RGBA8 for the upload path.
    const auto widen = [](std::span<const unsigned char> bytes, unsigned width, unsigned height, unsigned channels) {
        assets::Rgba8Image image{.extent = {width, height}, .pixels = {}};
        image.pixels.resize(std::size_t(width) * height * 4);
        for (std::size_t i = 0; i < std::size_t(width) * height; i++)
            for (unsigned c = 0; c < channels; c++)
                image.pixels[i * 4 + c] = bytes[i * channels + c];
        return image;
    };
    upload_images({{.data = assets::texture_from_image(widen(assets::smaa_area(), assets::smaa_area_width,
                                                             assets::smaa_area_height, assets::smaa_area_channels)),
                    .slot = Slot::smaa_area},
                   {.data = assets::texture_from_image(widen(assets::smaa_search(), assets::smaa_search_width,
                                                             assets::smaa_search_height, assets::smaa_search_channels)),
                    .slot = Slot::smaa_search}});
    create_pipelines();
    create_fixed_targets();
}

void Renderer::Impl::resize_galaxy(unsigned divisor) {
    divisor = std::clamp(divisor, 1u, 4u);
    if (divisor == galaxy_divisor || extent.width == 0)
        return;
    gpu::wait_idle(device);
    galaxy_divisor = divisor;
    frame_targets.galaxy.reset();
    frame_targets.galaxy = create_image(
        {.extent = {std::max(1u, extent.width / divisor), std::max(1u, extent.height / divisor)},
         .format = gpu::Format::rgba16_float,
         .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    bind(Slot::milky_way, frame_targets.galaxy);
}

void Renderer::Impl::resize(Extent2D new_extent, unsigned divisor) {
    divisor = std::clamp(divisor, 1u, 4u);
    if (extent == new_extent) {
        resize_galaxy(divisor);
        return;
    }
    galaxy_divisor = divisor;
    gpu::wait_idle(device);
    log::info("Resizing frame targets {}x{} -> {}x{}", extent.width, extent.height, new_extent.width,
              new_extent.height);
    frame_targets = {};
    extent = new_extent;
    history_valid = false;
    const auto color_usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment;
    const unsigned bloom_width = std::max(1u, extent.width / 4), bloom_height = std::max(1u, extent.height / 4);
    frame_targets.hdr = create_image({.extent = extent, .format = gpu::Format::rgba16_float, .usage = color_usage});
    frame_targets.depth = create_image(
        {.extent = extent,
         .format = gpu::Format::d32_float,
         .usage = gpu::TextureUsage::depth_stencil_attachment | gpu::TextureUsage::sampled});
    frame_targets.sun_visibility = create_image(
        {.extent = {1, 1}, .format = gpu::Format::rgba16_float, .usage = color_usage});
    frame_targets.bloom_a = create_image(
        {.extent = {bloom_width, bloom_height}, .format = gpu::Format::rgba16_float, .usage = color_usage});
    frame_targets.bloom_b = create_image(
        {.extent = {bloom_width, bloom_height}, .format = gpu::Format::rgba16_float, .usage = color_usage});
    frame_targets.final_image = create_image({.extent = extent,
                                              .format = gpu::Format::rgba8_srgb,
                                              .usage = color_usage | gpu::TextureUsage::transfer_source});
    frame_targets.ldr = create_image({.extent = extent, .format = gpu::Format::rgba8_srgb, .usage = color_usage});
    for (auto& image : frame_targets.history)
        image = create_image({.extent = extent, .format = gpu::Format::rgba16_float, .usage = color_usage});
    frame_targets.splat_mask = create_image(
        {.extent = extent, .format = gpu::Format::rgba16_float, .usage = color_usage});
    frame_targets.smaa_edges = create_image({.extent = extent, .format = gpu::Format::rg8_unorm, .usage = color_usage});
    frame_targets.smaa_weights = create_image(
        {.extent = extent, .format = gpu::Format::rgba8_unorm, .usage = color_usage});
    frame_targets.belt_dust = create_image({.extent = {std::max(1u, extent.width / 2), std::max(1u, extent.height / 2)},
                                            .format = gpu::Format::rgba16_float,
                                            .usage = color_usage});
    // The Milky Way's splat sum is smooth at its narrowest splat, so a fraction of the frame resolves it.
    frame_targets.galaxy = create_image(
        {.extent = {std::max(1u, extent.width / galaxy_divisor), std::max(1u, extent.height / galaxy_divisor)},
         .format = gpu::Format::rgba16_float,
         .usage = color_usage});
    bind(Slot::hdr, frame_targets.hdr);
    bind(Slot::bloom_a, frame_targets.bloom_a);
    bind(Slot::sun_visibility, frame_targets.sun_visibility);
    bind(Slot::milky_way, frame_targets.galaxy);
    bind(Slot::bloom_b, frame_targets.bloom_b);
    bind(Slot::final_image, frame_targets.final_image);
    bind(Slot::ldr, frame_targets.ldr);
    bind(Slot::depth, frame_targets.depth);
    bind(Slot::splat_mask, frame_targets.splat_mask);
    bind(Slot::smaa_edges, frame_targets.smaa_edges);
    bind(Slot::smaa_weights, frame_targets.smaa_weights);
    bind(Slot::belt_dust, frame_targets.belt_dust);
}

Renderer::Renderer(void* window, const SystemDescription& system, const std::filesystem::path& directory,
                   assets::ImageView hud, const RendererConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->init(window, system, directory, hud, config);
}

Renderer::~Renderer() = default;

Stats Renderer::stats() const {
    return impl_->stats;
}

} // namespace space::render
