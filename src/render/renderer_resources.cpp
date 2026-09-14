#include "render/renderer_impl.hpp"

#include "app/hud.hpp"
#include "assets/smaa.hpp"
#include "core/log.hpp"
#include "core/panic.hpp"
#include <algorithm>
#include <cstring>
#include <format>

namespace space::render {

namespace {
std::uint64_t padded_size(const assets::TextureMip& mip) {
    return (mip.bytes.size() + 15) & ~15ull;
}

} // namespace

Renderer::Impl::~Impl() {
    if (device)
        gpu::wait_idle(device);
    for (auto* pipeline : pipelines)
        gpu::destroy_pso(pipeline);
    for (auto& image : material_images)
        destroy(image);
    for (auto* image : {&hdr,          &depth,      &sun_visibility,  &bloom_a,         &bloom_b,
                        &final_image,  &ldr,        &shadow_map,      &luminance,       &history[0],
                        &history[1],   &belt_light, &belt_light_blur, &splat_mask,      &smaa_edges,
                        &smaa_weights, &belt_dust,  &belt_disc_light, &belt_disc_rocks, &galaxy})
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

std::uint64_t Renderer::Impl::upload_static(ByteView bytes) {
    static_cursor = (static_cursor + 15) & ~15ull;
    panic_if(static_cursor + bytes.size() > heap_layout.dynamic_offset, "static GPU heap exhausted");
    const auto address = reinterpret_cast<std::uint64_t>(data.range.gpu) + static_cursor;
    std::memcpy(data.range.cpu + static_cursor, bytes.data(), bytes.size());
    static_cursor += bytes.size();
    return address;
}

GpuImage Renderer::Impl::create_image(const ImageDesc& desc) {
    const gpu::TextureDesc texture_desc{.extent = {desc.extent.width, desc.extent.height, 1},
                                        .mip_levels = desc.mips,
                                        .format = desc.format,
                                        .usage = desc.usage};
    const auto size = gpu::get_texture_size_align(device, texture_desc);
    GpuImage result;
    result.heap = gpu::create_texture_heap(device, size.size);
    result.texture = gpu::create_texture(device, texture_desc, result.heap, 0);
    panic_if(!result.texture, "texture allocation failed ({}x{}, {} mips)", desc.extent.width, desc.extent.height,
             desc.mips);
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

// Creates one sampled texture per upload and streams every mip through
// a single reusable staging heap. Host-visible heaps live in device-local
// (BAR) memory on this backend, so the heap is capped and flushed in batches
// instead of sized to the whole set. Textures are created before
// begin_commands so the backend records their layout initialization first.
void Renderer::Impl::upload_images(std::span<Upload> uploads) {
    std::vector<GpuImage> images;
    images.reserve(uploads.size());
    std::uint64_t largest = 0;
    for (const auto& upload : uploads) {
        ORBITAL_ASSERT(assets::valid_texture(upload.data));
        const auto& base = upload.data.mips.front();
        images.push_back(create_image({.extent = base.extent,
                                       .format = texture_format(upload.data),
                                       .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::transfer_destination,
                                       .mips = unsigned(upload.data.mips.size())}));
        material_images.push_back(images.back());
        bind(upload.slot, images.back());
        std::uint64_t bytes = 0;
        for (const auto& mip : upload.data.mips)
            bytes += padded_size(mip);
        largest = std::max(largest, bytes);
    }
    auto staging = gpu::create_gpu_heap(device, std::max(largest, heap_layout.staging_budget));
    panic_if(!staging.range.cpu, "texture staging allocation failed");
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
        for (const auto& mip : uploads[i].data.mips)
            bytes += padded_size(mip);
        if (offset + bytes > staging.range.size)
            flush();
        if (!cmd)
            cmd = gpu::begin_commands(device);
        for (unsigned level = 0; level < uploads[i].data.mips.size(); level++) {
            const auto& mip = uploads[i].data.mips[level];
            std::memcpy(staging.range.cpu + offset, mip.bytes.data(), mip.bytes.size());
            const auto source = reinterpret_cast<std::uint64_t>(staging.range.gpu) + offset;
            gpu::copy_memory_to_texture(cmd, {reinterpret_cast<void*>(source), mip.bytes.size()}, images[i].texture,
                                        {.mip_level = level});
            offset += padded_size(mip);
        }
    }
    flush();
    gpu::destroy_gpu_heap(staging);
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
    timeline = gpu::create_timeline_semaphore(device);
    data = gpu::create_gpu_heap(device, heap_layout.static_heap);
    texture_descriptors = gpu::create_gpu_heap(device, caps.texture_descriptor_size * unsigned(Slot::count),
                                               gpu::MemoryType::texture_descriptor_heap);
    sampler_descriptors = gpu::create_gpu_heap(device, caps.sampler_descriptor_size * unsigned(SamplerSlot::count),
                                               gpu::MemoryType::sampler_descriptor_heap);
    panic_if(!data.range.cpu || !texture_descriptors.range.cpu || !sampler_descriptors.range.cpu,
             "GPU mapped heap allocation failed");
    luminance_readback = gpu::create_gpu_heap(device, targets::meter_size * targets::meter_size * sizeof(Float4),
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

void Renderer::Impl::create_fixed_targets() {
    shadow_map = create_image({.extent = {targets::shadow_map_size, targets::shadow_map_size},
                               .format = gpu::Format::d32_float,
                               .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::depth_stencil_attachment});
    bind(Slot::shadow_map, shadow_map);
    for (auto* map : {&belt_light, &belt_light_blur})
        *map = create_image({.extent = {targets::belt_light_map_size, targets::belt_light_map_size},
                             .format = gpu::Format::rgba16_float,
                             .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    bind(Slot::belt_light_map, belt_light);
    bind(Slot::belt_light_blur, belt_light_blur);
    belt_disc_light = create_image({.extent = {targets::belt_disc_map_size, targets::belt_disc_map_size},
                                    .format = gpu::Format::rgba16_float,
                                    .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    belt_disc_rocks = create_image({.extent = {targets::belt_disc_rock_map_size, targets::belt_disc_rock_map_size},
                                    .format = gpu::Format::rgba16_float,
                                    .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    bind(Slot::belt_disc_light, belt_disc_light);
    bind(Slot::belt_disc_rocks, belt_disc_rocks);
    luminance = create_image({.extent = {targets::meter_size, targets::meter_size},
                              .format = gpu::Format::rgba32_float,
                              .usage = gpu::TextureUsage::color_attachment | gpu::TextureUsage::transfer_source});
}

void Renderer::Impl::init(void* window, const SystemDescription& description,
                          const std::filesystem::path& base_directory, const RendererConfig& config) {
    system = description;
    directory = base_directory;
    belt_count_override = config.belt_count;
    panic_if(system.bodies.empty() || system.bodies.size() > max_body_count || system.belts.empty(),
             "the renderer needs 1 to {} bodies and a belt", max_body_count);
    body_count = unsigned(system.bodies.size());
    const auto find_class = [&](BodyClass body_class, const char* name) {
        for (unsigned i = 0; i < body_count; i++)
            if (system.bodies[i].body_class == body_class)
                return i;
        panic(std::format("the system has no {} body", name));
    };
    earth_index = find_class(BodyClass::Terrestrial, "terrestrial");
    giant_index = find_class(BodyClass::GasGiant, "gas giant");
    mars_index = find_class(BodyClass::Desert, "desert");
    create_device(window);
    create_samplers();
    create_meshes();
    build_belt(system.belts[0]);
    log::info("Loading planetary maps and scanned rock PBR materials...");
    load_materials();
    Uploads hud;
    hud.emplace_back();
    hud.back().data = assets::texture_from_images({assets::make_hud()});
    hud.back().slot = Slot::hud;
    upload_images(hud);
    // Embedded SMAA lookup tables, widened to RGBA8 for the upload path.
    const auto widen = [](std::span<const unsigned char> bytes, unsigned width, unsigned height, unsigned channels) {
        assets::Image image{.extent = {width, height}, .pixels = {}};
        image.pixels.resize(std::size_t(width) * height * 4);
        for (std::size_t i = 0; i < std::size_t(width) * height; i++)
            for (unsigned c = 0; c < channels; c++)
                image.pixels[i * 4 + c] = bytes[i * channels + c];
        return image;
    };
    Uploads smaa;
    smaa.emplace_back();
    smaa.back().data = assets::texture_from_images(
        {widen(assets::smaa_area(), assets::smaa_area_width, assets::smaa_area_height, assets::smaa_area_channels)});
    smaa.back().slot = Slot::smaa_area;
    smaa.emplace_back();
    smaa.back().data = assets::texture_from_images({widen(assets::smaa_search(), assets::smaa_search_width,
                                                          assets::smaa_search_height, assets::smaa_search_channels)});
    smaa.back().slot = Slot::smaa_search;
    upload_images(smaa);
    create_pipelines();
    create_fixed_targets();
}

void Renderer::Impl::resize_galaxy(unsigned divisor) {
    divisor = std::clamp(divisor, 1u, 4u);
    if (divisor == galaxy_divisor || extent.width == 0)
        return;
    gpu::wait_idle(device);
    galaxy_divisor = divisor;
    destroy(galaxy);
    galaxy = create_image({.extent = {std::max(1u, extent.width / divisor), std::max(1u, extent.height / divisor)},
                           .format = gpu::Format::rgba16_float,
                           .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    bind(Slot::milky_way, galaxy);
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
    for (auto* image : {&hdr, &depth, &sun_visibility, &bloom_a, &bloom_b, &final_image, &ldr, &history[0], &history[1],
                        &splat_mask, &smaa_edges, &smaa_weights, &belt_dust, &galaxy})
        destroy(*image);
    extent = new_extent;
    history_valid = false;
    const auto color_usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment;
    const unsigned bloom_width = std::max(1u, extent.width / 4), bloom_height = std::max(1u, extent.height / 4);
    hdr = create_image({.extent = extent, .format = gpu::Format::rgba16_float, .usage = color_usage});
    depth = create_image({.extent = extent,
                          .format = gpu::Format::d32_float,
                          .usage = gpu::TextureUsage::depth_stencil_attachment | gpu::TextureUsage::sampled});
    sun_visibility = create_image({.extent = {1, 1}, .format = gpu::Format::rgba16_float, .usage = color_usage});
    bloom_a = create_image(
        {.extent = {bloom_width, bloom_height}, .format = gpu::Format::rgba16_float, .usage = color_usage});
    bloom_b = create_image(
        {.extent = {bloom_width, bloom_height}, .format = gpu::Format::rgba16_float, .usage = color_usage});
    final_image = create_image({.extent = extent,
                                .format = gpu::Format::rgba8_srgb,
                                .usage = color_usage | gpu::TextureUsage::transfer_source});
    ldr = create_image({.extent = extent, .format = gpu::Format::rgba8_srgb, .usage = color_usage});
    for (auto& image : history)
        image = create_image({.extent = extent, .format = gpu::Format::rgba16_float, .usage = color_usage});
    splat_mask = create_image({.extent = extent, .format = gpu::Format::rgba16_float, .usage = color_usage});
    smaa_edges = create_image({.extent = extent, .format = gpu::Format::rg8_unorm, .usage = color_usage});
    smaa_weights = create_image({.extent = extent, .format = gpu::Format::rgba8_unorm, .usage = color_usage});
    belt_dust = create_image({.extent = {std::max(1u, extent.width / 2), std::max(1u, extent.height / 2)},
                              .format = gpu::Format::rgba16_float,
                              .usage = color_usage});
    // The Milky Way's splat sum is smooth at its narrowest splat, so a fraction of the frame resolves it.
    galaxy = create_image(
        {.extent = {std::max(1u, extent.width / galaxy_divisor), std::max(1u, extent.height / galaxy_divisor)},
         .format = gpu::Format::rgba16_float,
         .usage = color_usage});
    bind(Slot::hdr, hdr);
    bind(Slot::bloom_a, bloom_a);
    bind(Slot::sun_visibility, sun_visibility);
    bind(Slot::milky_way, galaxy);
    bind(Slot::bloom_b, bloom_b);
    bind(Slot::final_image, final_image);
    bind(Slot::ldr, ldr);
    bind(Slot::depth, depth);
    bind(Slot::splat_mask, splat_mask);
    bind(Slot::smaa_edges, smaa_edges);
    bind(Slot::smaa_weights, smaa_weights);
    bind(Slot::belt_dust, belt_dust);
}

Renderer::Renderer(void* window, const SystemDescription& system, const std::filesystem::path& directory,
                   const RendererConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->init(window, system, directory, config);
}

Renderer::~Renderer() = default;

Stats Renderer::stats() const {
    return impl_->stats;
}

} // namespace space::render
