#include "renderer.hpp"
#include "app/hud.hpp"
#include "assets/image.hpp"
#include "assets/kernels.hpp"
#include "assets/materials.hpp"
#include "gpu_types.hpp"
#include "scene/geometry.hpp"
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <vector>

namespace space::render {
namespace {
constexpr std::uint64_t heap_size = 48ull * 1024 * 1024;
constexpr std::uint64_t dynamic_base = 32ull * 1024 * 1024;
Float4 f4(Vec3d v, float w = 0) {
    return {float(v.x), float(v.y), float(v.z), w};
}
std::vector<std::uint32_t> read_spv(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error("Missing shader: " + path.string());
    auto size = f.tellg();
    if (size <= 0 || std::uint64_t(size) % 4)
        throw std::runtime_error("Invalid shader: " + path.string());
    std::vector<std::uint32_t> data(std::size_t(size) / 4);
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), size);
    if (!f || data[0] != 0x07230203)
        throw std::runtime_error("Invalid SPIR-V: " + path.string());
    return data;
}
void make_projection(FrameData& f, const Camera& camera, float aspect) {
    const auto r = camera.right(), u = camera.up(), d = camera.forward();
    float view[16] = {float(r.x), float(u.x), float(-d.x), 0, float(r.y), float(u.y), float(-d.y), 0,
                      float(r.z), float(u.z), float(-d.z), 0, 0,          0,          0,           1};
    // Positions passed to the GPU are camera-relative.
    const float tanHalf = float(std::tan(camera.vertical_fov * .5)), nearZ = .02f, farZ = 2000;
    float proj[16] = {
        1 / (aspect * tanHalf),        0, 0, 0, 0, -1 / tanHalf, 0, 0, 0, 0, farZ / (nearZ - farZ), -1, 0, 0,
        farZ * nearZ / (nearZ - farZ), 0};
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float v = 0;
            for (int k = 0; k < 4; k++)
                v += proj[k * 4 + row] * view[c * 4 + k];
            f.view_projection[c * 4 + row] = v;
        }
    f.right_tan = f4(r, tanHalf);
    f.up_aspect = f4(u, aspect);
    f.forward_exposure = f4(d, 1);
}
void make_light_projection(FrameData& f, Vec3d center, Vec3d sun, float halfSize) {
    Vec3d forward = normalized(center - sun), r = normalized(Vec3d{-forward.z, 0, forward.x});
    Vec3d u{r.y * forward.z - r.z * forward.y, r.z * forward.x - r.x * forward.z, r.x * forward.y - r.y * forward.x};
    Vec3d eye = center - forward * (halfSize * 3);
    float* m = f.light_projection;
    m[0] = float(r.x / halfSize);
    m[4] = float(r.y / halfSize);
    m[8] = float(r.z / halfSize);
    m[12] = float(-dot(r, eye) / halfSize);
    m[1] = float(-u.x / halfSize);
    m[5] = float(-u.y / halfSize);
    m[9] = float(-u.z / halfSize);
    m[13] = float(dot(u, eye) / halfSize);
    m[2] = float(forward.x / (halfSize * 6));
    m[6] = float(forward.y / (halfSize * 6));
    m[10] = float(forward.z / (halfSize * 6));
    m[14] = float(-dot(forward, eye) / (halfSize * 6));
    m[15] = 1;
}
} // namespace
struct Renderer::Impl {
    struct Image {
        gpu::TextureHeap heap{};
        gpu::Texture* texture{};
        gpu::RenderView* view{};
    };
    struct Mesh {
        std::uint64_t vertices{}, indices{};
        unsigned index_count{};
    };
    gpu::Device* device{};
    gpu::TimelineSemaphore* timeline{};
    std::uint64_t serial{};
    gpu::GpuHeap data{}, textures{}, samplers{}, luminance_readback{}, timestamps{};
    std::uint64_t cursor{};
    std::vector<Image> assets;
    std::vector<gpu::PSO*> pipelines;
    Image hdr{}, depth{}, bloomA{}, bloomB{}, finalImage{}, shadowImage{}, luminance{}, history[2]{};
    gpu::PSO *opaque{}, *cloud{}, *background{}, *atmosphere{}, *bloom{}, *post{}, *present{}, *shadow{}, *meter{},
        *temporal{};
    std::array<Mesh, 4> spheres{};
    std::array<Mesh, 4> rocks{};
    std::vector<geometry::AsteroidInstance> belt;
    struct BeltCluster {
        geometry::Vec3 center{};
        float radius{};
        std::vector<unsigned> indices;
    };
    std::vector<BeltCluster> belt_clusters;
    SystemDescription system;
    unsigned width{}, height{};
    Stats stats{};
    std::filesystem::path directory;
    unsigned frame_index{};
    float adapted_exposure = 1;
    bool meter_pending = false;
    FrameData previous_frame{};
    Vec3d previous_camera{};
    bool history_valid = false;
    ~Impl() {
        if (device)
            gpu::wait_idle(device);
        for (auto p : pipelines)
            gpu::destroy_pso(p);
        for (auto& i : assets)
            destroy(i);
        destroy(hdr);
        destroy(depth);
        destroy(bloomA);
        destroy(bloomB);
        destroy(finalImage);
        destroy(shadowImage);
        destroy(luminance);
        destroy(history[0]);
        destroy(history[1]);
        gpu::destroy_gpu_heap(data);
        gpu::destroy_gpu_heap(textures);
        gpu::destroy_gpu_heap(samplers);
        gpu::destroy_gpu_heap(luminance_readback);
        gpu::destroy_gpu_heap(timestamps);
        gpu::destroy_timeline_semaphore(timeline);
        gpu::destroy_device(device);
    }
    void destroy(Image& i) {
        gpu::destroy_render_view(i.view);
        gpu::destroy_texture(i.texture);
        gpu::destroy_texture_heap(i.heap);
        i = {};
    }
    std::uint64_t upload(const void* p, std::size_t bytes) {
        cursor = (cursor + 15) & ~15ull;
        if (cursor + bytes > dynamic_base)
            throw std::runtime_error("Static GPU heap exhausted");
        auto address = reinterpret_cast<std::uint64_t>(data.range.gpu) + cursor;
        std::memcpy(data.range.cpu + cursor, p, bytes);
        cursor += bytes;
        return address;
    }
    Mesh mesh(const geometry::Mesh& m) {
        std::vector<Vertex> v;
        v.reserve(m.vertices.size());
        for (auto a : m.vertices)
            v.push_back({{a.position.x, a.position.y, a.position.z, 1}, {a.normal.x, a.normal.y, a.normal.z, 0}});
        return {upload(v.data(), v.size() * sizeof(Vertex)), upload(m.indices.data(), m.indices.size() * 4),
                unsigned(m.indices.size())};
    }
    Image image(unsigned w, unsigned h, gpu::Format format, gpu::TextureUsage usage, unsigned mips = 1) {
        gpu::TextureDesc desc{.extent = {w, h, 1}, .mip_levels = mips, .format = format, .usage = usage};
        auto size = gpu::get_texture_size_align(device, desc);
        Image result;
        result.heap = gpu::create_texture_heap(device, size.size);
        result.texture = gpu::create_texture(device, desc, result.heap, 0);
        if (!result.texture) {
            destroy(result);
            throw std::runtime_error("Texture allocation failed");
        }
        if ((static_cast<unsigned>(usage) & static_cast<unsigned>(gpu::TextureUsage::color_attachment |
                                                                  gpu::TextureUsage::depth_stencil_attachment)) != 0)
            result.view = gpu::create_render_view(result.texture);
        return result;
    }
    void descriptor(unsigned slot, const Image& i) {
        const auto& caps = gpu::get_device_caps(device);
        gpu::write_texture_descriptor(device, textures.range.cpu + slot * caps.texture_descriptor_size, i.texture,
                                      gpu::TextureDescriptorType::sampled);
    }
    struct Upload {
        std::vector<assets::Image> mips;
        unsigned slot;
    };
    // Creates one sampled RGBA8 texture per upload and streams every mip through
    // a single reusable staging heap. Host-visible heaps live in device-local
    // (BAR) memory on this backend, so the heap is capped and flushed in batches
    // instead of sized to the whole set. Textures are created before
    // begin_commands so the backend records their layout initialization first.
    void upload_images(const std::vector<Upload>& uploads) {
        constexpr std::uint64_t staging_budget = 64ull * 1024 * 1024;
        const auto padded = [](const assets::Image& mip) { return (mip.pixels.size() + 15) & ~15ull; };
        std::vector<Image> images;
        std::uint64_t largest = 0;
        for (const auto& upload : uploads) {
            const auto& base = upload.mips.at(0);
            images.push_back(image(base.width, base.height, gpu::Format::rgba8_unorm,
                                   gpu::TextureUsage::sampled | gpu::TextureUsage::transfer_destination,
                                   unsigned(upload.mips.size())));
            assets.push_back(images.back());
            descriptor(upload.slot, images.back());
            std::uint64_t bytes = 0;
            for (const auto& mip : upload.mips)
                bytes += padded(mip);
            largest = std::max(largest, bytes);
        }
        auto staging = gpu::create_gpu_heap(device, std::max(largest, staging_budget));
        if (!staging.range.cpu)
            throw std::runtime_error("Texture staging allocation failed");
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
                bytes += padded(mip);
            if (offset + bytes > staging.range.size)
                flush();
            if (!cmd)
                cmd = gpu::begin_commands(device);
            for (unsigned level = 0; level < uploads[i].mips.size(); level++) {
                const auto& mip = uploads[i].mips[level];
                std::memcpy(staging.range.cpu + offset, mip.pixels.data(), mip.pixels.size());
                const auto source = reinterpret_cast<std::uint64_t>(staging.range.gpu) + offset;
                gpu::copy_memory_to_texture(cmd, {reinterpret_cast<void*>(source), mip.pixels.size()},
                                            images[i].texture, {.mip_level = level});
                offset += padded(mip);
            }
        }
        flush();
        gpu::destroy_gpu_heap(staging);
    }
    void upload_image(std::vector<assets::Image> mips, unsigned slot) {
        std::vector<Upload> single;
        single.push_back({std::move(mips), slot});
        upload_images(single);
    }
    gpu::PSO* pipeline(const char* vs, const char* fs, gpu::Format format, bool depthTest, bool blend) {
        auto v = read_spv(directory / "shaders" / (std::string(vs) + ".vertex.spv")),
             f = read_spv(directory / "shaders" / (std::string(fs) + ".fragment.spv"));
        gpu::BlendState blending{};
        if (blend)
            blending = {.enabled = true,
                        .color = {gpu::BlendFactor::source_alpha, gpu::BlendFactor::one_minus_source_alpha},
                        .alpha = {gpu::BlendFactor::one, gpu::BlendFactor::one_minus_source_alpha}};
        gpu::ColorTargetDesc target{.format = format, .blend = blending};
        auto p = gpu::create_graphics_pso(
            device, {.vertex_spirv = v,
                     .fragment_spirv = f,
                     .color_targets = {&target, 1},
                     .depth_format = depthTest ? gpu::Format::d32_float : gpu::Format::undefined});
        if (!p)
            throw std::runtime_error(std::string("Pipeline failed: ") + fs);
        pipelines.push_back(p);
        return p;
    }
    // Decodes and mip-filters every material on a bounded worker pool, then
    // uploads them all in one batch on this thread. Unused descriptor slots
    // point at the first map.
    void load_materials() {
        struct Source {
            const char* file;
            unsigned slot;
            bool srgb, luminance_to_alpha = false, normal_map = false;
        };
        static constexpr Source sources[] = {
            {"earth_albedo.png", 3, true},
            {"gas_albedo.png", 4, true},
            {"earth_clouds.png", 5, false, true},
            {"earth_normal.png", 7, false, false, true},
            {"earth_specular.png", 8, false},
            {"earth_night.png", 9, true},
            {"moon_albedo.png", 10, true},
            {"rock_albedo.png", 11, true},
            {"rock_normal.png", 12, false, false, true},
            {"rock_roughness.png", 13, false},
        };
        constexpr std::size_t count = std::size(sources);
        const auto start = std::chrono::steady_clock::now();
        // Leave one core for this thread and cap the pool: decoding and filtering
        // are memory-bound enough that more workers stop helping.
        const unsigned cores = std::thread::hardware_concurrency();
        const std::size_t workers = std::min(count, std::size_t(std::clamp(cores > 1 ? cores - 1 : 1u, 1u, 8u)));
        std::vector<std::vector<assets::Image>> chains(count);
        std::atomic<std::size_t> next{0};
        std::vector<std::future<void>> pool;
        for (std::size_t worker = 0; worker < workers; worker++)
            pool.push_back(std::async(std::launch::async, [&] {
                for (std::size_t i = next++; i < count; i = next++)
                    chains[i] = assets::load_material(directory / "assets/materials" / sources[i].file,
                                                      {.encoding = sources[i].srgb ? assets::MaterialEncoding::SRGB
                                                                                   : assets::MaterialEncoding::Linear,
                                                       .luminance_to_alpha = sources[i].luminance_to_alpha,
                                                       .normal_map = sources[i].normal_map});
            }));
        for (auto& worker : pool)
            worker.get();
        std::vector<Upload> uploads;
        for (std::size_t i = 0; i < count; i++)
            uploads.push_back({std::move(chains[i]), sources[i].slot});
        const auto first_asset = assets.size();
        upload_images(uploads);
        bool used[24]{};
        for (const auto& source : sources)
            used[source.slot] = true;
        for (unsigned slot = 0; slot < 24; slot++)
            if (!used[slot])
                descriptor(slot, assets[first_asset]);
        std::cout
            << "Loaded " << count << " materials in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
            << " ms on " << workers << " workers (" << assets::kernels::backend() << ")\n";
    }
    void init(void* hwnd, const SystemDescription& sys, const std::filesystem::path& dir) {
        system = sys;
        directory = dir;
        auto init = gpu::create_device(
            {.window = hwnd, .swapchain_format = gpu::Format::bgra8_srgb, .timestamp_query_count = 16});
        device = init.device;
        if (!device)
            throw std::runtime_error(
                "Vulkan device creation failed. Check console for missing features / driver errors.");
        const auto& caps = gpu::get_device_caps(device);
        std::cout << "GPU: " << caps.device_name << " | conventional NoGraphicsAPI backend\n";
        if (!caps.conventional_descriptor_backend)
            throw std::runtime_error("This demo's shaders require the conventional descriptor backend build option.");
        timeline = gpu::create_timeline_semaphore(device);
        data = gpu::create_gpu_heap(device, heap_size);
        textures = gpu::create_gpu_heap(device, caps.texture_descriptor_size * 24,
                                        gpu::MemoryType::texture_descriptor_heap);
        samplers = gpu::create_gpu_heap(device, caps.sampler_descriptor_size * 4,
                                        gpu::MemoryType::sampler_descriptor_heap);
        if (!data.range.cpu || !textures.range.cpu || !samplers.range.cpu)
            throw std::runtime_error("GPU mapped heap allocation failed");
        for (unsigned i = 0; i < 4; i++)
            gpu::write_sampler_descriptor(
                device, samplers.range.cpu + i * caps.sampler_descriptor_size,
                {.address_u = (i == 1 || i == 3) ? gpu::AddressMode::repeat : gpu::AddressMode::clamp_to_edge,
                 .address_v = i == 3 ? gpu::AddressMode::repeat : gpu::AddressMode::clamp_to_edge,
                 .address_w = gpu::AddressMode::clamp_to_edge,
                 .anisotropic = i == 1 || i == 3,
                 .compare_enabled = i == 2});
        for (unsigned i = 0; i < 4; i++) {
            spheres[i] = mesh(geometry::generate_sphere(32u << i, 16u << i));
            rocks[i] = mesh(geometry::generate_rock(71 + i * 37, 2 + i));
        }
        belt = geometry::generate_belt(sys.belts[0].seed, 65000, float(sys.belts[0].inner_radius),
                                       float(sys.belts[0].outer_radius), float(sys.belts[0].thickness));
        // The seeded belt order defines quality tiers, but is spatially random.
        // Build a separate index into compact polar cells without reordering IDs.
        constexpr unsigned angleBins = 128, radialBins = 8, heightBins = 4,
                           binCount = angleBins * radialBins * heightBins;
        std::array<std::vector<unsigned>, binCount> bins;
        const float inner = float(sys.belts[0].inner_radius), outer = float(sys.belts[0].outer_radius),
                    thickness = float(sys.belts[0].thickness);
        for (unsigned i = 0; i < belt.size(); ++i) {
            const auto& p = belt[i].position;
            float angle = std::atan2(p.z, p.x);
            if (angle < 0)
                angle += float(2 * 3.14159265358979323846);
            float radial = std::sqrt(p.x * p.x + p.z * p.z);
            unsigned a = std::min(angleBins - 1, unsigned(angle / float(2 * 3.14159265358979323846) * angleBins));
            unsigned r = std::min(
                radialBins - 1,
                unsigned(std::clamp((radial - inner) / std::max(outer - inner, 1e-4f), 0.f, .999999f) * radialBins));
            unsigned h = std::min(
                heightBins - 1,
                unsigned(std::clamp((p.y + thickness * .5f) / std::max(thickness, 1e-4f), 0.f, .999999f) * heightBins));
            bins[(h * radialBins + r) * angleBins + a].push_back(i);
        }
        belt_clusters.reserve(binCount);
        for (auto& ids : bins)
            if (!ids.empty()) {
                geometry::Vec3 lo = belt[ids[0]].position, hi = lo;
                for (unsigned id : ids) {
                    const auto& p = belt[id].position;
                    lo.x = std::min(lo.x, p.x);
                    lo.y = std::min(lo.y, p.y);
                    lo.z = std::min(lo.z, p.z);
                    hi.x = std::max(hi.x, p.x);
                    hi.y = std::max(hi.y, p.y);
                    hi.z = std::max(hi.z, p.z);
                }
                geometry::Vec3 centre = (lo + hi) * .5f;
                float bound = 0;
                for (unsigned id : ids) {
                    auto d = belt[id].position - centre;
                    const auto& s = belt[id].scale;
                    float sizeTail = id % 137 == 0 ? 9.f : (id % 23 == 0 ? 4.f : 1.f);
                    float rockRadius = .025f * std::max(s.x, std::max(s.y, s.z)) * sizeTail;
                    bound = std::max(bound, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) + rockRadius);
                }
                belt_clusters.push_back({centre, bound, std::move(ids)});
            }
        std::cout << "Loading planetary maps and scanned rock PBR materials...\n";
        load_materials();
        upload_image({assets::make_hud()}, 14);
        opaque = pipeline("surface", "surface", gpu::Format::rgba16_float, true, false);
        cloud = pipeline("surface", "surface", gpu::Format::rgba16_float, true, true);
        background = pipeline("fullscreen", "background", gpu::Format::rgba16_float, true, false);
        atmosphere = pipeline("fullscreen", "atmosphere", gpu::Format::rgba16_float, false, true);
        bloom = pipeline("fullscreen", "post", gpu::Format::rgba16_float, false, false);
        post = pipeline("fullscreen", "post", gpu::Format::rgba8_srgb, false, false);
        present = pipeline("fullscreen", "post", gpu::Format::bgra8_srgb, false, false);
        meter = pipeline("fullscreen", "post", gpu::Format::rgba32_float, false, false);
        temporal = pipeline("fullscreen", "temporal", gpu::Format::rgba16_float, false, false);
        auto shadowVS = read_spv(directory / "shaders/surface.vertex.spv");
        shadow = gpu::create_graphics_pso(device,
                                          {.vertex_spirv = shadowVS,
                                           .depth_format = gpu::Format::d32_float,
                                           .rasterization = {.depth_bias_constant = 1, .depth_bias_slope = 1.5f}});
        if (!shadow)
            throw std::runtime_error("Shadow pipeline creation failed");
        pipelines.push_back(shadow);
        shadowImage = image(2048, 2048, gpu::Format::d32_float,
                            gpu::TextureUsage::sampled | gpu::TextureUsage::depth_stencil_attachment);
        descriptor(15, shadowImage);
        luminance = image(16, 16, gpu::Format::rgba32_float,
                          gpu::TextureUsage::color_attachment | gpu::TextureUsage::transfer_source);
        luminance_readback = gpu::create_gpu_heap(device, 16 * 16 * 16, gpu::MemoryType::readback);
        timestamps = gpu::create_gpu_heap(device, 64, gpu::MemoryType::readback);
    }
    void resize(unsigned w, unsigned h) {
        if (width == w && height == h)
            return;
        gpu::wait_idle(device);
        destroy(hdr);
        destroy(depth);
        destroy(bloomA);
        destroy(bloomB);
        destroy(finalImage);
        destroy(history[0]);
        destroy(history[1]);
        width = w;
        height = h;
        history_valid = false;
        auto colorUsage = gpu::TextureUsage::sampled | gpu::TextureUsage::color_attachment;
        hdr = image(w, h, gpu::Format::rgba16_float, colorUsage);
        depth = image(w, h, gpu::Format::d32_float,
                      gpu::TextureUsage::depth_stencil_attachment | gpu::TextureUsage::sampled);
        bloomA = image(std::max(1u, w / 4), std::max(1u, h / 4), gpu::Format::rgba16_float, colorUsage);
        bloomB = image(std::max(1u, w / 4), std::max(1u, h / 4), gpu::Format::rgba16_float, colorUsage);
        finalImage = image(w, h, gpu::Format::rgba8_srgb, colorUsage | gpu::TextureUsage::transfer_source);
        for (auto& hst : history)
            hst = image(w, h, gpu::Format::rgba16_float, colorUsage);
        descriptor(0, hdr);
        descriptor(1, bloomA);
        descriptor(2, bloomB);
        descriptor(6, finalImage);
        descriptor(18, depth);
    }
    void fullpass(gpu::CommandBuffer* c, Image& dst, gpu::PSO* p, Root root, bool preserve = false) {
        gpu::ColorAttachment a{.render_view = dst.view, .load = preserve ? gpu::LoadOp::load : gpu::LoadOp::clear};
        gpu::begin_render_pass(c, {.colors = {&a, 1}});
        gpu::bind_pso(c, p);
        gpu::draw(c, root, 3);
        gpu::end_render_pass(c);
        gpu::barrier(c, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                     gpu::Access::shader_read);
    }
};
Renderer::Renderer(void* hwnd, const SystemDescription& s, const std::filesystem::path& d)
    : impl_(std::make_unique<Impl>()) {
    impl_->init(hwnd, s, d);
}
Renderer::~Renderer() = default;
const char* Renderer::device_name() const {
    return gpu::get_device_caps(impl_->device).device_name;
}
Stats Renderer::stats() const {
    return impl_->stats;
}
bool Renderer::draw(const Camera& camera, const std::vector<BodyState>& bodies, double time, float exposure, bool high,
                    bool overlay, bool autoExposure) {
    auto start = std::chrono::steady_clock::now();
    auto& s = *impl_;
    gpu::wait_timeline({s.timeline, s.serial});
    if (s.frame_index) {
        const auto t = reinterpret_cast<const std::uint64_t*>(s.timestamps.range.cpu);
        double scale = gpu::get_device_caps(s.device).timestamp_period_ns * 1e-6;
        s.stats.gpu_ms = (t[4] - t[0]) * scale;
        s.stats.shadow_ms = (t[1] - t[0]) * scale;
        s.stats.surface_ms = (t[2] - t[1]) * scale;
        s.stats.atmosphere_ms = (t[3] - t[2]) * scale;
        s.stats.post_ms = (t[4] - t[3]) * scale;
    }
    if (s.meter_pending) {
        auto values = reinterpret_cast<float*>(s.luminance_readback.range.cpu);
        double sum = 0;
        unsigned count = 0;
        for (unsigned i = 0; i < 256; i++)
            if (values[i * 4 + 1] > .004f) {
                sum += values[i * 4];
                count++;
            }
        float target = count ? std::clamp(.18f / std::exp(float(sum / count)), .75f, 1.75f) : 1.f;
        s.adapted_exposure += (target - s.adapted_exposure) * .08f;
        s.meter_pending = false;
    }
    auto extent = gpu::get_drawable_extent(s.device);
    if (!extent.x || !extent.y)
        return false;
    s.resize(extent.x, extent.y);
    auto swap = gpu::acquire(s.device);
    if (!swap.render_view)
        return false;
    FrameData frame{};
    make_projection(frame, camera, float(s.width) / s.height);
    constexpr float jitter[8][2] = {{0, -.166667f},    {-.25f, .166667f},   {.25f, -.388889f}, {-.375f, -.055556f},
                                    {.125f, .277778f}, {-.125f, -.277778f}, {.375f, .055556f}, {-.4375f, .388889f}};
    frame.jitter = {jitter[s.frame_index % 8][0], jitter[s.frame_index % 8][1], s.previous_frame.jitter.x,
                    s.previous_frame.jitter.y};
    for (unsigned col = 0; col < 4; col++) {
        frame.view_projection[col * 4] += 2 * frame.jitter.x / s.width * frame.view_projection[col * 4 + 3];
        frame.view_projection[col * 4 + 1] += 2 * frame.jitter.y / s.height * frame.view_projection[col * 4 + 3];
    }
    std::memcpy(frame.previous_projection, s.previous_frame.view_projection, 64);
    frame.previous_camera_delta = f4(camera.position - s.previous_camera, s.history_valid ? 1.f : 0.f);
    frame.previous_forward = s.previous_frame.forward_exposure;
    if (length(camera.position - s.previous_camera) > 10 ||
        dot(camera.forward(), {s.previous_frame.forward_exposure.x, s.previous_frame.forward_exposure.y,
                               s.previous_frame.forward_exposure.z}) < .7)
        frame.previous_camera_delta.w = 0;
    unsigned historyWrite = s.frame_index % 2;
    s.descriptor(16, s.history[historyWrite]);
    s.descriptor(17, s.history[1 - historyWrite]);
    frame.camera_time = {0, 0, 0, float(time)};
    frame.forward_exposure.w = exposure * (autoExposure ? s.adapted_exposure : 1);
    frame.sun = f4(s.system.star.position - camera.position, 3.0f);
    unsigned shadowBody = length(bodies[1].position - camera.position) < 100 ? 1 : 0;
    make_light_projection(frame, bodies[shadowBody].position - camera.position,
                          s.system.star.position - camera.position, shadowBody ? 70.f : 12.f);
    frame.options = {float(s.width), float(s.height), high ? 1.f : 0.f, overlay ? 1.f : 0.f};
    std::vector<Instance> instances;
    instances.reserve(s.belt.size() + 3);
    for (unsigned i = 0; i < 3; i++) {
        frame.bodies[i] = f4(bodies[i].position - camera.position, float(bodies[i].radius));
        instances.push_back({frame.bodies[i],
                             {0, float(bodies[i].rotation_angle) + (i == 0 ? -.95f : 0.f),
                              float(s.system.bodies[i].axial_tilt), float(i)},
                             {1, 1, 1, 1}});
    }
    const Vec3d sunDirection = normalized(s.system.star.position - camera.position);
    double forward = dot(sunDirection, camera.forward());
    frame.screen_sun = {
        float(dot(sunDirection, camera.right()) / (std::max(forward, .001) * frame.right_tan.w * frame.up_aspect.w)),
        float(-dot(sunDirection, camera.up()) / (std::max(forward, .001) * frame.right_tan.w)), forward > 0 ? 1.f : 0.f,
        .008f};
    for (auto b : bodies) {
        auto v = b.position - camera.position;
        double along = dot(v, sunDirection);
        if (along > 0 && length(v - sunDirection * along) < b.radius)
            frame.screen_sun.z = 0;
    }
    if (std::abs(frame.screen_sun.x) > 1.3 || std::abs(frame.screen_sun.y) > 1.3)
        frame.screen_sun.z = 0;
    std::array<std::vector<Instance>, 4> groups;
    std::vector<Instance> distant;
    unsigned limit = high ? unsigned(s.belt.size()) : 35000;
    double beltAngle = time * .008, beltCos = std::cos(beltAngle), beltSin = std::sin(beltAngle);
    const double tanY = frame.right_tan.w, tanX = tanY * frame.up_aspect.w;
    const double planeXScale = std::sqrt(1 + tanX * tanX), planeYScale = std::sqrt(1 + tanY * tanY);
    auto beltTransform = [&](geometry::Vec3 v) {
        double bx = v.x * beltCos - v.z * beltSin, bz = v.x * beltSin + v.z * beltCos;
        return Vec3d{bx, v.y * .7 - bz * .36, bz * .933};
    };
    auto fullyOccluded = [&](Vec3d target, float targetRadius) {
        double targetDistance = length(target);
        if (targetDistance <= targetRadius)
            return false;
        Vec3d targetDirection = target * (1.0 / targetDistance);
        double targetAngle = std::asin(std::clamp(double(targetRadius) / targetDistance, 0.0, 1.0));
        for (unsigned bodyIndex = 0; bodyIndex < 3; ++bodyIndex) {
            Vec3d occluder = bodies[bodyIndex].position - camera.position;
            double occluderDistance = length(occluder), occluderRadius = bodies[bodyIndex].radius;
            if (occluderDistance <= occluderRadius ||
                targetDistance - targetRadius <= occluderDistance + occluderRadius)
                continue;
            double separation = std::acos(
                std::clamp(dot(targetDirection, occluder * (1.0 / occluderDistance)), -1.0, 1.0));
            double occluderAngle = std::asin(std::clamp(occluderRadius / occluderDistance, 0.0, 1.0));
            if (separation + targetAngle < occluderAngle)
                return true;
        }
        return false;
    };
    for (const auto& cluster : s.belt_clusters) {
        // The tilt/compression matrix has norm below 1.293; 1.30 therefore
        // conservatively transforms the raw-space cluster sphere at all angles.
        Vec3d clusterPosition = bodies[1].position + beltTransform(cluster.center) - camera.position;
        float clusterRadius = cluster.radius * 1.30f;
        double clusterZ = dot(clusterPosition, camera.forward());
        if (clusterZ < -clusterRadius)
            continue;
        const double clusterPadding = clusterRadius + std::max(clusterZ, 0.0) * tanY * 4 / s.height;
        if (std::abs(dot(clusterPosition, camera.right())) > clusterZ * tanX + clusterPadding * planeXScale ||
            std::abs(dot(clusterPosition, camera.up())) > clusterZ * tanY + clusterPadding * planeYScale)
            continue;
        if (fullyOccluded(clusterPosition, clusterRadius))
            continue;
        for (unsigned i : cluster.indices) {
            if (i >= limit)
                continue;
            const auto& b = s.belt[i];
            Vec3d local = beltTransform(b.position);
            // A sparse large-body tail exposes the fractured silhouettes between
            // the much more numerous small rocks. Stable IDs keep tiers nested.
            float sizeTail = i % 137 == 0 ? 9.f : (i % 23 == 0 ? 4.f : 1.f);
            Vec3d p = bodies[1].position + local - camera.position;
            float radius = b.scale.x * .025f * sizeTail;
            double z = dot(p, camera.forward());
            if (z < -radius)
                continue;
            float pixelRadius = radius * s.height / (2 * float(std::max(z, .1)) * frame.right_tan.w);
            if (pixelRadius < .06f)
                continue;
            // Plane normals are not unit length; scale sphere support accordingly.
            // Two pixels also cover temporal jitter and expanded tiny billboards.
            const double padding = radius + std::max(z, 0.0) * tanY * 4 / s.height;
            if (std::abs(dot(p, camera.right())) > z * tanX + padding * planeXScale ||
                std::abs(dot(p, camera.up())) > z * tanY + padding * planeYScale)
                continue;
            if (pixelRadius < 1.2f) {
                float displayRadius = 1.2f;
                float coverage = pixelRadius * pixelRadius / (displayRadius * displayRadius);
                coverage *= std::clamp((pixelRadius - .06f) / .06f, 0.f, 1.f);
                distant.push_back({f4(p, radius * displayRadius / pixelRadius),
                                   {0, 0, 0, 3},
                                   {.8f + float(i % 7) * .05f, .84f, .78f, coverage}});
                continue;
            }
            unsigned lod = radius * s.height / (std::max(z, .1) * frame.right_tan.w) > 5 ? b.variant : 0;
            groups[lod].push_back({f4(p, radius),
                                   {b.rotation.x, float(b.rotation.y + time * .08), b.rotation.z, 3},
                                   {.8f + float(i % 7) * .05f, .84f, .78f, 1}});
        }
    }
    std::array<unsigned, 4> bases{}, counts{};
    for (unsigned i = 0; i < 4; i++) {
        bases[i] = unsigned(instances.size());
        counts[i] = unsigned(groups[i].size());
        instances.insert(instances.end(), groups[i].begin(), groups[i].end());
    }
    unsigned distantBase = unsigned(instances.size());
    instances.insert(instances.end(), distant.begin(), distant.end());
    s.stats.visible_asteroids = unsigned(instances.size() - 3);
    s.stats.triangles = 0;
    auto addr = reinterpret_cast<std::uint64_t>(s.data.range.gpu) + dynamic_base;
    std::memcpy(s.data.range.cpu + dynamic_base, &frame, sizeof(frame));
    std::memcpy(s.data.range.cpu + dynamic_base + 512, instances.data(), instances.size() * sizeof(Instance));
    Root root{addr, 0, addr + 512, 0, 2};
    auto cmd = gpu::begin_commands(s.device);
    auto stamp = [&](unsigned index) {
        gpu::write_timestamp(cmd, reinterpret_cast<gpu::uint64*>(s.timestamps.range.gpu + index * 8));
    };
    stamp(0);
    gpu::set_texture_descriptor_heap(cmd, gpu::gpu_range(s.textures));
    gpu::set_sampler_descriptor_heap(cmd, gpu::gpu_range(s.samplers));
    gpu::barrier(cmd, gpu::Stage::all_commands,
                 gpu::Access::shader_read | gpu::Access::color_write | gpu::Access::depth_stencil_write,
                 gpu::Stage::all_commands,
                 gpu::Access::color_write | gpu::Access::depth_stencil_write | gpu::Access::shader_read);
    gpu::begin_render_pass(cmd, {.depth = {.render_view = s.shadowImage.view, .load = gpu::LoadOp::clear}});
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, s.shadow);
    for (unsigned i = 0; i < 3; i++) {
        const auto& m = s.spheres[2];
        root.base = i;
        root.vertices = m.vertices;
        gpu::draw_indexed(cmd, root, {reinterpret_cast<void*>(m.indices), std::uint64_t(m.index_count) * 4},
                          gpu::IndexType::uint32, m.index_count);
    }
    for (unsigned i = 0; i < 4; i++)
        if (counts[i]) {
            const auto& m = s.rocks[i];
            root.base = bases[i];
            root.vertices = m.vertices;
            gpu::draw_indexed(cmd, root, {reinterpret_cast<void*>(m.indices), std::uint64_t(m.index_count) * 4},
                              gpu::IndexType::uint32, m.index_count, counts[i]);
        }
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    stamp(1);
    root.mode = 0;
    gpu::ColorAttachment color{.render_view = s.hdr.view, .load = gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd,
                           {.colors = {&color, 1}, .depth = {.render_view = s.depth.view, .load = gpu::LoadOp::clear}});
    gpu::bind_pso(cmd, s.background);
    gpu::draw(cmd, root, 3);
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, s.opaque);
    for (unsigned i = 0; i < 3; i++) {
        float distance = float(length(bodies[i].position - camera.position));
        unsigned lod = geometry::select_lod(float(bodies[i].radius) * s.height / (distance * frame.right_tan.w), 2);
        const auto& mesh = s.spheres[lod];
        root.base = i;
        root.vertices = mesh.vertices;
        gpu::draw_indexed(cmd, root, {reinterpret_cast<void*>(mesh.indices), std::uint64_t(mesh.index_count) * 4},
                          gpu::IndexType::uint32, mesh.index_count);
        s.stats.triangles += mesh.index_count / 3;
    }
    for (unsigned i = 0; i < 4; i++)
        if (counts[i]) {
            const auto& mesh = s.rocks[i];
            root.base = bases[i];
            root.vertices = mesh.vertices;
            gpu::draw_indexed(cmd, root, {reinterpret_cast<void*>(mesh.indices), std::uint64_t(mesh.index_count) * 4},
                              gpu::IndexType::uint32, mesh.index_count, counts[i]);
            s.stats.triangles += mesh.index_count / 3 * counts[i];
        }
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = false});
    gpu::bind_pso(cmd, s.cloud);
    if (!distant.empty()) {
        root.base = distantBase;
        root.mode = 3;
        gpu::draw(cmd, root, 6, unsigned(distant.size()));
        s.stats.triangles += unsigned(distant.size()) * 2;
    }
    root.base = 0;
    root.mode = 1;
    root.vertices = s.spheres[3].vertices;
    gpu::draw_indexed(cmd, root,
                      {reinterpret_cast<void*>(s.spheres[3].indices), std::uint64_t(s.spheres[3].index_count) * 4},
                      gpu::IndexType::uint32, s.spheres[3].index_count);
    gpu::end_render_pass(cmd);
    stamp(2);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::color_output,
                 gpu::Access::color_read | gpu::Access::color_write);
    root.mode = 0;
    root.base = 1;
    s.fullpass(cmd, s.hdr, s.atmosphere, root, true);
    root.base = 0;
    s.fullpass(cmd, s.hdr, s.atmosphere, root, true);
    stamp(3);
    gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    s.fullpass(cmd, s.history[historyWrite], s.temporal, root);
    root.mode = 1;
    s.fullpass(cmd, s.bloomA, s.bloom, root);
    root.mode = 2;
    s.fullpass(cmd, s.bloomB, s.bloom, root);
    root.mode = 0;
    s.fullpass(cmd, s.finalImage, s.post, root);
    if (s.frame_index++ % 16 == 0) {
        root.mode = 4;
        s.fullpass(cmd, s.luminance, s.meter, root);
        gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::transfer,
                     gpu::Access::transfer_read);
        gpu::copy_texture_to_memory(cmd, s.luminance.texture, gpu::gpu_range(s.luminance_readback));
        gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
        s.meter_pending = true;
    }
    color = {.render_view = swap.render_view, .load = gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd, {.colors = {&color, 1}});
    root.mode = 3;
    gpu::bind_pso(cmd, s.present);
    gpu::draw(cmd, root, 3);
    gpu::end_render_pass(cmd);
    stamp(4);
    gpu::submit_and_present(s.device, {cmd}, {s.timeline, ++s.serial});
    s.previous_frame = frame;
    s.previous_camera = camera.position;
    s.history_valid = true;
    s.stats.frame_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}
void Renderer::capture(const std::filesystem::path& path) {
    auto& s = *impl_;
    if (!s.width)
        return;
    gpu::wait_timeline({s.timeline, s.serial});
    auto readback = gpu::create_gpu_heap(s.device, std::uint64_t(s.width) * s.height * 4, gpu::MemoryType::readback);
    if (!readback.range.cpu)
        throw std::runtime_error("Screenshot readback allocation failed");
    auto c = gpu::begin_commands(s.device);
    gpu::barrier(c, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::transfer,
                 gpu::Access::transfer_read);
    gpu::copy_texture_to_memory(c, s.finalImage.texture, gpu::gpu_range(readback));
    gpu::barrier(c, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
    gpu::submit({c}, {s.timeline, ++s.serial});
    gpu::wait_timeline({s.timeline, s.serial});
    const auto* p = reinterpret_cast<const unsigned char*>(readback.range.cpu);
    std::vector<std::uint8_t> rgb(std::size_t(s.width) * s.height * 3);
    for (std::size_t i = 0; i < std::size_t(s.width) * s.height; i++)
        for (unsigned channel = 0; channel < 3; channel++)
            rgb[i * 3 + channel] = p[i * 4 + channel];
    gpu::destroy_gpu_heap(readback);
    if (!assets::save_png(path, s.width, s.height, 3, rgb.data()))
        throw std::runtime_error("Cannot save screenshot");
}
} // namespace space::render
