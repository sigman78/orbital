#include "render/renderer_impl.hpp"

#include "assets/smaa.hpp"
#include "core/log.hpp"
#include "core/panic_if.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <initializer_list>
#include <iterator>
#include <string>

namespace space::render {

namespace {
std::uint64_t padded_size(const assets::TextureMip& mip) {
    return (mip.bytes.size() + 15) & ~15ull;
}

// Everything that reads the static records: the draws (indices, vertices, rock and
// sky tables) and the culling compute.
constexpr AccessScope static_read_access{gpu::Stage::indirect | gpu::Stage::index_input | gpu::Stage::vertex |
                                             gpu::Stage::fragment | gpu::Stage::compute,
                                         gpu::Access::index_read | gpu::Access::shader_read};

void flush_upload(gpu::CommandBuffer*& cmd, std::uint64_t& offset, SubmissionTimeline& submissions,
                  AccessScope readers) {
    if (!cmd)
        return;
    synchronize(cmd, access::transfer_write, readers);
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
    static_upload.staging.reset(); // empty after start-up; here for an init that stopped early
    buffers = {};
    timings.reset();
    submissions.reset();
    gpu::destroy_device(device);
}

// Copies the bytes into the device-only static heap through a bounded staging heap.
// The copies are submitted when the staging fills and by finish_static_uploads.
std::uint64_t Renderer::Impl::upload_static(ByteView bytes) {
    static_cursor = (static_cursor + 15) & ~15ull;
    panic_if(static_cursor + bytes.size() > heap_layout.static_budget, "static GPU heap exhausted");
    auto& upload = static_upload;
    const auto padded = (bytes.size() + 15) & ~15ull;
    if (upload.offset + padded > upload.staging.range().size)
        finish_static_uploads();
    if (!upload.staging.range().cpu) {
        upload.staging = UniqueGpuHeap::create(device, std::max<std::uint64_t>(padded, heap_layout.staging_budget));
        panic_if(!upload.staging.range().cpu, "static staging allocation failed");
    }
    std::memcpy(upload.staging.range().cpu + upload.offset, bytes.data(), bytes.size());
    if (!upload.cmd)
        upload.cmd = gpu::begin_commands(device);
    const auto address = reinterpret_cast<std::uint64_t>(buffers.static_data.range().gpu) + static_cursor;
    gpu::copy_memory(upload.cmd, {upload.staging.range().gpu + upload.offset, bytes.size()},
                     {reinterpret_cast<void*>(address), bytes.size()});
    upload.offset += padded;
    static_cursor += bytes.size();
    return address;
}

// Submits the pending static copies and releases the staging heap.
void Renderer::Impl::finish_static_uploads() {
    flush_upload(static_upload.cmd, static_upload.offset, submissions, static_read_access);
    static_upload.staging.reset();
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
    auto staging = UniqueGpuHeap::create(device, std::max(largest, heap_layout.staging_budget));
    panic_if(!staging.range().cpu, "texture staging allocation failed");
    gpu::CommandBuffer* cmd = nullptr;
    std::uint64_t offset = 0;
    for (std::size_t i = 0; i < uploads.size(); i++) {
        std::uint64_t bytes = 0;
        for (const auto& mip : uploads[i].data.mips)
            bytes += padded_size(mip);
        if (offset + bytes > staging.range().size)
            flush_upload(cmd, offset, submissions, access::fragment_sample);
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
    flush_upload(cmd, offset, submissions, access::fragment_sample);
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
    const auto init = gpu::create_device({.window = window,
                                          .swapchain_format = gpu::Format::bgra8_srgb,
                                          .timestamp_query_count = GpuTimings::timestamp_count});
    device = init.device;
    panic_if(!device, "Vulkan device creation failed; check the console for missing features or driver errors");
    const auto& caps = gpu::get_device_caps(device);
    log::info("GPU: {} | conventional NoGraphicsAPI backend", caps.device_name);
    stats.output.hdr_metadata = caps.hdr_metadata;
    panic_if(!caps.conventional_descriptor_backend,
             "the demo's shaders require the conventional descriptor backend build option");
    submissions.initialize(device);
    buffers.static_data = UniqueGpuHeap::create(device, heap_layout.static_budget, gpu::MemoryType::gpu_only);
    buffers.data = UniqueGpuHeap::create(device, heap_layout.mapped_size());
    belt_capacity = std::max(high_quality.belt_count, belt_count_override);
    buffers.cull_device = UniqueGpuHeap::create(device, heap_layout.cull_size(belt_capacity, body_count),
                                                gpu::MemoryType::gpu_only);
    buffers.cull_readback = UniqueGpuHeap::create(device, sizeof(CullScratch), gpu::MemoryType::readback);
    buffers.belt_state = UniqueGpuHeap::create(device, 2ull * belt_capacity * belt_state_stride,
                                               gpu::MemoryType::gpu_only);
    buffers.belt_state_staging = UniqueGpuHeap::create(device, std::uint64_t(belt_capacity) * belt_state_stride);
    panic_if(!buffers.static_data.range().gpu || !buffers.cull_device.range().gpu ||
                 !buffers.cull_readback.range().cpu || !buffers.belt_state.range().gpu ||
                 !buffers.belt_state_staging.range().cpu,
             "device heap allocation failed");
    log::info(
        "Buffer heaps: {} KiB device-only static, {} KiB mapped, {} KiB device-only culling, {} + {} KiB rock state",
        buffers.static_data.range().size / 1024, buffers.data.range().size / 1024,
        buffers.cull_device.range().size / 1024, buffers.belt_state.range().size / 1024,
        buffers.belt_state_staging.range().size / 1024);
    buffers.texture_descriptors = UniqueGpuHeap::create(device, caps.texture_descriptor_size * unsigned(Slot::count),
                                                        gpu::MemoryType::texture_descriptor_heap);
    buffers.sampler_descriptors = UniqueGpuHeap::create(
        device, caps.sampler_descriptor_size * unsigned(SamplerSlot::count), gpu::MemoryType::sampler_descriptor_heap);
    panic_if(!buffers.data.range().cpu || !buffers.texture_descriptors.range().cpu ||
                 !buffers.sampler_descriptors.range().cpu,
             "GPU mapped heap allocation failed");
    // The exposure histogram: accumulated on the device, zeroed from a mapped source each cycle, read back once per
    // cycle.
    buffers.meter_device = UniqueGpuHeap::create(device, sizeof(MeterHistogram), gpu::MemoryType::gpu_only);
    buffers.meter_zero = UniqueGpuHeap::create(device, sizeof(MeterHistogram));
    buffers.meter_readback = UniqueGpuHeap::create(device, sizeof(MeterHistogram), gpu::MemoryType::readback);
    panic_if(!buffers.meter_device.range().gpu || !buffers.meter_zero.range().cpu ||
                 !buffers.meter_readback.range().cpu,
             "exposure meter heap allocation failed");
    std::memset(buffers.meter_zero.range().cpu, 0, sizeof(MeterHistogram));
    timings.initialize(device);
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
    // Where the start-up time goes, phase by phase, for the log line at the end.
    auto phase_start = std::chrono::steady_clock::now();
    std::string phases;
    const auto phase = [&](const char* name) {
        const auto now = std::chrono::steady_clock::now();
        std::format_to(std::back_inserter(phases), "{}{} {} ms", phases.empty() ? "" : ", ", name,
                       std::chrono::duration_cast<std::chrono::milliseconds>(now - phase_start).count());
        phase_start = now;
    };
    const auto init_start = phase_start;
    create_device(window);
    phase("device");
    create_samplers();
    create_meshes();
    phase("meshes");
    build_belt(system.belts.front());
    phase("belt");
    log::info("Loading planetary maps and scanned rock PBR materials...");
    load_materials();
    finish_static_uploads();
    phase("materials and sky");
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
    phase("tables");
    create_pipelines();
    phase("pipelines");
    create_fixed_targets();
    phase("targets");
    log::info(
        "Renderer ready in {} ms: {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - init_start).count(),
        phases);
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

// The flare stack target follows the panel's resolution choice; coarser reads as defocus.
void Renderer::Impl::resize_flare(unsigned divisor) {
    divisor = std::clamp(divisor, 2u, 8u);
    if (divisor == flare_divisor || extent.width == 0)
        return;
    gpu::wait_idle(device);
    flare_divisor = divisor;
    frame_targets.flare.reset();
    frame_targets.flare = create_image(
        {.extent = {std::max(1u, extent.width / divisor), std::max(1u, extent.height / divisor)},
         .format = gpu::Format::rgba16_float,
         .usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment});
    bind(Slot::flare, frame_targets.flare);
}

// The swapchain's output. HDR modes need the surface to offer the format and
// colour space pair, which it does only with the OS presenting in HDR; an
// unsupported request is reported through the stats and the output stays as
// it was. A switch recreates the swapchain on the next acquire and the two
// intermediates, which for HDR are 16-bit float holding the composite's linear
// display value over the headroom, so the spatial anti-aliasing sees the same
// values it does in SDR and the present pass scales them.
void Renderer::Impl::set_hdr_output(HdrOutput mode) {
    if (mode == hdr_output || mode == hdr_requested)
        return;
    hdr_requested = mode;
    gpu::Format format = gpu::Format::bgra8_srgb;
    gpu::ColorSpace color_space = gpu::ColorSpace::srgb_nonlinear;
    if (mode == HdrOutput::ScRgb) {
        format = gpu::Format::rgba16_float;
        color_space = gpu::ColorSpace::extended_srgb_linear;
    } else if (mode == HdrOutput::Hdr10) {
        format = gpu::Format::rgb10a2_unorm;
        color_space = gpu::ColorSpace::hdr10_st2084;
    }
    static constexpr const char* names[] = {"SDR", "scRGB", "HDR10"};
    if (mode != HdrOutput::Off && !gpu::surface_format_supported(device, format, color_space)) {
        log::warn("{} output is not offered by the display surface (is HDR on in the OS?); staying at {}",
                  names[unsigned(mode)], names[unsigned(hdr_output)]);
        stats.output.hdr_unsupported = true;
        return;
    }
    stats.output.hdr_unsupported = false;
    gpu::wait_idle(device);
    gpu::set_swapchain_output(device, format, color_space);
    hdr_output = mode;
    stats.output.hdr_output = mode;
    if (extent.width) {
        const auto color_usage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment;
        frame_targets.final_image.reset();
        frame_targets.ldr.reset();
        frame_targets.final_image = create_image({.extent = extent,
                                                  .format = intermediate_format(),
                                                  .usage = color_usage | gpu::TextureUsage::transfer_source});
        frame_targets.ldr = create_image({.extent = extent, .format = intermediate_format(), .usage = color_usage});
        bind(Slot::final_image, frame_targets.final_image);
        bind(Slot::ldr, frame_targets.ldr);
    }
    log::info("Output: {} ({}){}", names[unsigned(mode)],
              mode == HdrOutput::ScRgb   ? "16-bit float, extended linear sRGB"
              : mode == HdrOutput::Hdr10 ? "10-bit, SMPTE ST 2084"
                                         : "8-bit sRGB",
              mode == HdrOutput::Off      ? ""
              : stats.output.hdr_metadata ? ", mastering metadata sent"
                                          : ", no metadata path");
}

// SMPTE ST 2086 metadata for an HDR swapchain: the display's own range when
// the OS reported it (mastering the content to the display it is shown on),
// otherwise the panel's peak; the content light level is the curve's peak and
// the frame average the paper white, since a mostly dark sky never exceeds it.
void Renderer::Impl::update_hdr_metadata(const ToneSettings& tone, const DisplaySettings& display) {
    if (!hdr() || !stats.output.hdr_metadata) {
        hdr_metadata_valid = false;
        return;
    }
    gpu::HdrMetadata metadata;
    if (hdr_output == HdrOutput::ScRgb) { // scRGB carries Rec. 709 primaries
        metadata.red_x = .640f, metadata.red_y = .330f;
        metadata.green_x = .300f, metadata.green_y = .600f;
        metadata.blue_x = .150f, metadata.blue_y = .060f;
    }
    metadata.max_luminance = display.max_nits > 0 ? display.max_nits : tone.peak_nits;
    metadata.min_luminance = display.min_nits;
    metadata.max_content_light_level = tone.peak_nits;
    metadata.max_frame_average_light_level = std::min(tone.paper_white_nits, metadata.max_luminance);
    if (hdr_metadata_valid && std::memcmp(&metadata, &hdr_metadata_sent, sizeof(metadata)) == 0)
        return;
    gpu::set_hdr_metadata(device, metadata);
    hdr_metadata_sent = metadata;
    hdr_metadata_valid = true;
}

void Renderer::Impl::collect_memory_stats() {
    const auto images = [](std::initializer_list<const GpuImage*> list) {
        MemoryPool pool;
        for (const auto* image : list)
            if (image->bytes()) {
                pool.bytes += image->bytes();
                pool.count++;
            }
        pool.used = pool.bytes;
        return pool;
    };
    const auto heap = [](MemoryPool& pool, const UniqueGpuHeap& buffer) {
        if (buffer.range().size) {
            pool.bytes += buffer.range().size;
            pool.count++;
        }
    };
    auto& memory = stats.memory;
    memory.frame_targets = images({&frame_targets.hdr, &frame_targets.depth, &frame_targets.sun_visibility,
                                   &frame_targets.bloom_a, &frame_targets.bloom_b, &frame_targets.flare,
                                   &frame_targets.final_image, &frame_targets.ldr, &frame_targets.history[0],
                                   &frame_targets.history[1], &frame_targets.splat_mask, &frame_targets.smaa_edges,
                                   &frame_targets.smaa_weights, &frame_targets.belt_dust, &frame_targets.galaxy});
    memory.fixed_targets = images({&fixed_targets.shadow_map, &fixed_targets.belt_light, &fixed_targets.belt_light_blur,
                                   &fixed_targets.belt_disc_light, &fixed_targets.belt_disc_rocks});
    memory.materials = {};
    for (const auto& image : material_images)
        if (image.bytes()) {
            memory.materials.bytes += image.bytes();
            memory.materials.count++;
        }
    memory.materials.used = memory.materials.bytes;
    // The static heap fills from the front once; the mapped heap is the per-frame region
    // and the UI, always in use.
    memory.static_data = {};
    heap(memory.static_data, buffers.static_data);
    memory.static_data.used = static_cursor;
    memory.mapped = {};
    heap(memory.mapped, buffers.data);
    heap(memory.mapped, buffers.belt_state_staging);
    memory.mapped.used = memory.mapped.bytes;
    memory.device_buffers = {};
    heap(memory.device_buffers, buffers.cull_device);
    heap(memory.device_buffers, buffers.belt_state);
    heap(memory.device_buffers, buffers.meter_device);
    memory.device_buffers.used = memory.device_buffers.bytes;
    memory.readback = {};
    heap(memory.readback, buffers.cull_readback);
    heap(memory.readback, buffers.meter_readback);
    heap(memory.readback, buffers.meter_zero);
    heap(memory.readback, buffers.texture_descriptors);
    heap(memory.readback, buffers.sampler_descriptors);
    memory.readback.used = memory.readback.bytes;
}

void Renderer::Impl::resize(Extent2D new_extent, unsigned divisor, unsigned flare) {
    divisor = std::clamp(divisor, 1u, 4u);
    if (extent == new_extent) {
        resize_galaxy(divisor);
        resize_flare(flare);
        return;
    }
    galaxy_divisor = divisor;
    flare_divisor = std::clamp(flare, 2u, 8u);
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
    frame_targets.flare = create_image(
        {.extent = {std::max(1u, extent.width / flare_divisor), std::max(1u, extent.height / flare_divisor)},
         .format = gpu::Format::rgba16_float,
         .usage = color_usage});
    frame_targets.final_image = create_image(
        {.extent = extent, .format = intermediate_format(), .usage = color_usage | gpu::TextureUsage::transfer_source});
    frame_targets.ldr = create_image({.extent = extent, .format = intermediate_format(), .usage = color_usage});
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
    bind(Slot::flare, frame_targets.flare);
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

const Stats& Renderer::stats() const {
    return impl_->stats;
}

} // namespace space::render
