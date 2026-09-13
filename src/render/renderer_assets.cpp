#include "render/renderer_impl.hpp"

#include "assets/kernels.hpp"
#include "assets/material_catalog.hpp"
#include "assets/materials.hpp"
#include "assets/texture.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <thread>

namespace space::render {

namespace {
constexpr unsigned decode_workers_max = 8; // upper bound for the material decode pool

struct MaterialSource {
    const char* file;
    Slot slot;
    bool optional = false; // missing on disk binds a neutral map with a warning instead of a panic
};

constexpr MaterialSource material_sources[] = {{"earth_albedo.png", Slot::earth_albedo},
                                               {"gas_albedo.png", Slot::gas_albedo},
                                               {"earth_clouds.png", Slot::earth_clouds},
                                               {"earth_normal.png", Slot::earth_normal},
                                               {"earth_specular.png", Slot::earth_specular},
                                               {"earth_night.png", Slot::earth_night},
                                               {"moon_albedo.png", Slot::moon_albedo},
                                               {"rock_albedo.png", Slot::rock_albedo},
                                               {"rock_normal.png", Slot::rock_normal},
                                               {"rock_roughness.png", Slot::rock_roughness},
                                               {"mars_albedo.png", Slot::mars_albedo},
                                               {"mars_normal.png", Slot::mars_normal},
                                               {"moon_normal.png", Slot::moon_normal},
                                               {"rock_face_albedo.png", Slot::rock_face_albedo},
                                               {"rock_face_normal.png", Slot::rock_face_normal},
                                               {"rock_face_roughness.png", Slot::rock_face_roughness},
                                               {"rock_boulder_albedo.png", Slot::rock_boulder_albedo},
                                               {"rock_boulder_normal.png", Slot::rock_boulder_normal},
                                               {"rock_boulder_roughness.png", Slot::rock_boulder_roughness},
                                               {"gas_flow.png", Slot::gas_flow, true},
                                               {"gas_detail.png", Slot::gas_detail, true},
                                               {"gas_relief.png", Slot::gas_relief, true},
                                               {"gas_polar.png", Slot::gas_polar, true},
                                               {"gas_polar_flow.png", Slot::gas_polar_flow, true}};
constexpr std::size_t material_count = std::size(material_sources);
static_assert(material_count <= inline_upload_count);

void append_vertices(std::vector<Vertex>& out, const geometry::Mesh& mesh) {
    for (const auto& v : mesh.vertices)
        out.push_back({{v.position.x, v.position.y, v.position.z, 1}, {v.normal.x, v.normal.y, v.normal.z, 0}});
}

} // namespace

GpuMesh Renderer::Impl::upload_mesh(const geometry::Mesh& mesh) {
    std::vector<Vertex> vertices;
    vertices.reserve(mesh.vertices.size());
    append_vertices(vertices, mesh);
    return {.vertices = upload_static(bytes_of(vertices)),
            .indices = upload_static(bytes_of(mesh.indices)),
            .index_count = unsigned(mesh.indices.size())};
}

// One vertex and one index range for the whole rock library, so every rock
// group is a first-index/vertex-offset slice and one multi-draw covers them all.
void Renderer::Impl::upload_rock_pool(std::span<const geometry::Mesh> meshes) {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    for (std::size_t group = 0; group < meshes.size(); group++) {
        rocks[group] = {.index_count = unsigned(meshes[group].indices.size()),
                        .first_index = unsigned(indices.size()),
                        .vertex_offset = unsigned(vertices.size())};
        append_vertices(vertices, meshes[group]);
        indices.insert(indices.end(), meshes[group].indices.begin(), meshes[group].indices.end());
    }
    rock_pool = {.vertices = upload_static(bytes_of(vertices)),
                 .indices = upload_static(bytes_of(indices)),
                 .index_count = unsigned(indices.size())};
    for (auto& rock : rocks) {
        rock.vertices = rock_pool.vertices;
        rock.indices = rock_pool.indices;
    }
}

void Renderer::Impl::create_meshes() {
    for (unsigned lod = 0; lod < geometry::lod_count; lod++)
        spheres[lod] = upload_mesh(geometry::generate_sphere(32u << lod, 16u << lod));
    constexpr std::uint32_t rock_seed_base = 71, rock_seed_stride = 37;
    std::vector<geometry::Mesh> library(rock_group_count);
    for (unsigned shape = 0; shape < geometry::rock_shape_count; shape++)
        for (unsigned level = 0; level < geometry::rock_level_count; level++)
            library[rock_group(shape, level)] = geometry::generate_rock(rock_seed_base + shape * rock_seed_stride,
                                                                        level);
    upload_rock_pool(library);
    // Moonlets are irregular bodies: each gets its own seeded rock at full detail.
    for (unsigned i = 0; i < body_count; i++)
        if (system.bodies[i].body_class == BodyClass::Moonlet)
            moonlet_meshes[i] = upload_mesh(
                geometry::generate_rock(std::uint32_t(system.bodies[i].material_seed), geometry::rock_level_count - 1));
}

const GpuMesh& Renderer::Impl::body_mesh(unsigned body, unsigned lod) const {
    return system.bodies[body].body_class == BodyClass::Moonlet ? moonlet_meshes[body] : spheres[lod];
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
                                         std::size_t(std::clamp(cores > 1 ? cores - 1 : 1u, 1u, decode_workers_max)));
    assets::TextureSupport support;
    support.bc7 = gpu::supports_texture_format(device, gpu::Format::bc7_unorm,
                                               gpu::TextureUsage::sampled | gpu::TextureUsage::transfer_destination);
    for (const auto block : {4u, 6u, 8u, 12u}) {
        assets::TextureData probe{
            .format = assets::TextureFormat::ASTC, .mips = {}, .block_x = block, .block_y = block};
        support.astc_blocks[block] = gpu::supports_texture_format(
            device, texture_format(probe), gpu::TextureUsage::sampled | gpu::TextureUsage::transfer_destination);
    }
    std::atomic<unsigned> cached_count{0};
    Uploads uploads;
    for (std::size_t i = 0; i < material_count; i++)
        uploads.emplace_back();
    std::atomic<std::size_t> next{0};
    SmallVec<std::future<void>, decode_workers_max> pool;
    for (std::size_t worker = 0; worker < workers; worker++)
        pool.push_back(std::async(std::launch::async, [&] {
            for (std::size_t i = next++; i < material_count; i = next++) {
                const auto& source = material_sources[i];
                const auto path = directory / "assets/materials" / source.file;
                const auto desc = assets::material_description(source.file);
                auto cached = assets::load_texture_cache(path, desc, support);
                if (cached) {
                    uploads[i] = {.data = std::move(*cached), .slot = source.slot};
                    ++cached_count;
                    continue;
                }
                if (source.optional && !std::filesystem::exists(path)) {
                    log::warn("{} is missing; run tools/import-assets.ps1 to bake it (its effect is off)", source.file);
                    if (source.slot == Slot::gas_polar)
                        polar_caps = false;
                    assets::Image neutral{.extent = {4, 4}, .pixels = {}};
                    neutral.pixels.resize(4 * 4 * 4);
                    for (std::size_t p = 0; p < 16; p++) {
                        neutral.pixels[p * 4] = neutral.pixels[p * 4 + 1] = 128; // zero signed flow
                        neutral.pixels[p * 4 + 3] = 255;
                    }
                    uploads[i] = {.data = assets::texture_from_images({std::move(neutral)}), .slot = source.slot};
                    continue;
                }
                uploads[i] = {.data = assets::texture_from_images(assets::load_material(path, desc)),
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
    log::info("Texture cache: {} of {} materials", cached_count.load(), material_count);
    load_stars();
    load_splats();
}

// The Bright Star Catalogue baked by tools/bake-stars.py: a 16-byte header
// ("STAR", count, record size, reserved) and 32-byte records the star pass
// reads as vertices. Optional: without it the sky keeps its procedural stars.
void Renderer::Impl::load_stars() {
    const auto path = directory / "assets/materials/stars_bsc5.bin";
    const auto bytes = file::read(path);
    if (!bytes) {
        log::warn("stars_bsc5.bin is missing; run tools/import-assets.ps1 to bake it (the sky stays procedural)");
        return;
    }
    constexpr std::size_t header = 16, record = 32;
    if (bytes->size() < header || std::memcmp(bytes->data(), "STAR", 4) != 0) {
        log::warn("stars_bsc5.bin is not a star record file; ignored");
        return;
    }
    std::uint32_t count = 0, size = 0;
    std::memcpy(&count, bytes->data() + 4, 4);
    std::memcpy(&size, bytes->data() + 8, 4);
    if (size != record || bytes->size() < header + std::size_t(count) * record) {
        log::warn("stars_bsc5.bin has an unexpected layout; ignored");
        return;
    }
    star_data = upload_static(ByteView(bytes->data() + header, std::size_t(count) * record));
    star_count = count;
    log::info("Loaded {} catalogue stars", count);
}

// The Milky Way as Gaussian splats fitted to ESO's panorama by tools/bake-splats.py:
// a 16-byte header ("SPLT", count, record size, table length), 64-byte records
// the galaxy pass reads as two vertices each, then the cell table (uints) that
// buckets them by direction. Optional: without it the sky keeps its procedural band.
void Renderer::Impl::load_splats() {
    const auto path = directory / "assets/materials/milky_way_splats.bin";
    const auto bytes = file::read(path);
    if (!bytes) {
        log::warn("milky_way_splats.bin is missing; run tools/import-assets.ps1 to fit it (the band stays procedural)");
        return;
    }
    constexpr std::size_t header = 16, record = 64;
    if (bytes->size() < header || std::memcmp(bytes->data(), "SPLT", 4) != 0) {
        log::warn("milky_way_splats.bin is not a splat record file; ignored");
        return;
    }
    std::uint32_t count = 0, size = 0, table = 0;
    std::memcpy(&count, bytes->data() + 4, 4);
    std::memcpy(&size, bytes->data() + 8, 4);
    std::memcpy(&table, bytes->data() + 12, 4);
    const std::size_t payload = std::size_t(count) * record + std::size_t(table) * 4;
    if (size != record || table < 2 || bytes->size() < header + payload) {
        log::warn("milky_way_splats.bin has an unexpected layout; ignored");
        return;
    }
    splat_data = upload_static(ByteView(bytes->data() + header, payload));
    splat_count = count;
    log::info("Loaded {} Milky Way splats ({} KB with their cell table)", count, (payload + 512) / 1024);
}

} // namespace space::render
