#include "render/renderer_impl.hpp"

#include "core/file.hpp"
#include "core/panic_if.hpp"
#include <cstring>
#include <format>

namespace space::render {

namespace {
std::vector<std::uint32_t> read_spirv(const std::filesystem::path& path) {
    const auto bytes = file::read(path);
    panic_if(!bytes || bytes->empty() || bytes->size() % 4, "missing or invalid shader: {}", path.string());
    std::vector<std::uint32_t> words(bytes->size() / 4);
    std::memcpy(words.data(), bytes->data(), bytes->size());
    constexpr std::uint32_t spirv_magic = 0x07230203;
    panic_if(words[0] != spirv_magic, "not a SPIR-V module: {}", path.string());
    return words;
}

} // namespace

gpu::PSO* Renderer::Impl::create_pipeline(const PipelineDesc& desc) {
    const auto vertex = read_spirv(directory / "shaders" / std::format("{}.vertex.spv", desc.vertex_shader));
    const auto fragment = read_spirv(directory / "shaders" / std::format("{}.fragment.spv", desc.fragment_shader));
    gpu::BlendState blending{};
    if (desc.blend == Blend::alpha)
        blending = {.enabled = true,
                    .color = {gpu::BlendFactor::source_alpha, gpu::BlendFactor::one_minus_source_alpha},
                    .alpha = {gpu::BlendFactor::one, gpu::BlendFactor::one_minus_source_alpha}};
    else if (desc.blend == Blend::additive)
        blending = {.enabled = true,
                    .color = {gpu::BlendFactor::one, gpu::BlendFactor::one},
                    .alpha = {gpu::BlendFactor::one, gpu::BlendFactor::one}};
    else if (desc.blend == Blend::premultiplied)
        blending = {.enabled = true,
                    .color = {gpu::BlendFactor::one, gpu::BlendFactor::one_minus_source_alpha},
                    .alpha = {gpu::BlendFactor::one, gpu::BlendFactor::one_minus_source_alpha}};
    const gpu::ColorTargetDesc target{.format = desc.color_format, .blend = blending};
    auto* pipeline = gpu::create_graphics_pso(
        device, {.vertex_spirv = vertex,
                 .fragment_spirv = fragment,
                 .color_targets = {&target, 1},
                 .depth_format = desc.has_depth_attachment ? gpu::Format::d32_float : gpu::Format::undefined});
    panic_if(!pipeline, "pipeline creation failed for {} + {}", desc.vertex_shader, desc.fragment_shader);
    pipelines.emplace_back(pipeline);
    return pipeline;
}

void Renderer::Impl::create_pipelines() {
    using gpu::Format;
    const auto make = [&](const char* vertex, const char* fragment, Format format, bool has_depth_attachment = false,
                          Blend blend = Blend::none) {
        return create_pipeline({.vertex_shader = vertex,
                                .fragment_shader = fragment,
                                .color_format = format,
                                .has_depth_attachment = has_depth_attachment,
                                .blend = blend});
    };
    // Scene, sky and atmosphere pipelines.
    pso.scene.surface_earth = make("surface", "surface_earth", Format::rgba16_float, true);
    pso.scene.surface_giant = make("surface", "surface_giant", Format::rgba16_float, true);
    pso.scene.surface_airless = make("surface", "surface_airless", Format::rgba16_float, true);
    pso.scene.surface_rock = make("surface", "surface_rock", Format::rgba16_float, true);
    pso.scene.cloud = make("surface", "surface_earth", Format::rgba16_float, true, Blend::alpha);
    pso.scene.background = make("fullscreen", "background", Format::rgba16_float, true);
    pso.scene.galaxy = make("fullscreen", "galaxy", Format::rgba16_float);
    pso.scene.atmosphere = make("fullscreen", "atmosphere", Format::rgba16_float, false, Blend::alpha);
    pso.scene.motes = make("motes", "motes", Format::rgba16_float, true, Blend::additive);
    pso.scene.stars = make("stars", "stars", Format::rgba16_float, true, Blend::additive);
    // Belt meshes, splats and dust pipelines.
    pso.belt.billboard = make("surface", "surface_rock", Format::rgba16_float, true, Blend::alpha);
    pso.belt.splat = make("beltsplat", "beltsplat", Format::rgba16_float, false, Blend::additive);
    pso.belt.blur = make("fullscreen", "beltblur", Format::rgba16_float);
    pso.belt.splat_mask = make("surface", "surface_rock", Format::rgba16_float, true, Blend::premultiplied);
    pso.belt.dust = make("fullscreen", "dust", Format::rgba16_float);
    pso.belt.dust_blend = make("fullscreen", "dust", Format::rgba16_float, false, Blend::premultiplied);
    pso.belt.disc = make("fullscreen", "disc", Format::rgba16_float);
    pso.belt.disc_splat = make("discsplat", "discsplat", Format::rgba16_float, false, Blend::additive);
    // Temporal, tone mapping and spatial anti-aliasing pipelines.
    pso.post.sun_visibility = make("fullscreen", "sun_visibility", Format::rgba16_float);
    pso.post.flare = make("fullscreen", "flare", Format::rgba16_float);
    pso.post.bloom = make("fullscreen", "bloom", Format::rgba16_float);
    pso.post.composite = make("fullscreen", "composite", Format::rgba8_srgb);
    pso.post.composite_hdr = make("fullscreen", "composite", Format::rgba16_float);
    pso.post.present = make("fullscreen", "present", Format::bgra8_srgb);
    pso.post.present_scrgb = make("fullscreen", "present", Format::rgba16_float);
    pso.post.present_hdr10 = make("fullscreen", "present", Format::rgb10a2_unorm);
    pso.post.fxaa = make("fullscreen", "fxaa_pass", Format::rgba8_srgb);
    pso.post.fxaa_hdr = make("fullscreen", "fxaa_pass", Format::rgba16_float);
    pso.post.temporal = make("fullscreen", "temporal", Format::rgba16_float);
    pso.post.smaa_edges = make("fullscreen", "smaa", Format::rg8_unorm);
    pso.post.smaa_weights = make("fullscreen", "smaa", Format::rgba8_unorm);
    pso.post.smaa_blend = make("fullscreen", "smaa", Format::rgba8_srgb);
    pso.post.smaa_blend_hdr = make("fullscreen", "smaa", Format::rgba16_float);
    // Overlay pipeline.
    pso.ui = make("ui", "ui", Format::bgra8_srgb, false, Blend::alpha);
    pso.ui_scrgb = make("ui", "ui", Format::rgba16_float, false, Blend::alpha);
    pso.ui_hdr10 = make("ui", "ui", Format::rgb10a2_unorm, false, Blend::alpha);
    pso.belt.cull = gpu::create_compute_pso(device, read_spirv(directory / "shaders/cull.compute.spv"));
    panic_if(!pso.belt.cull, "compute pipeline creation failed: cull");
    pipelines.emplace_back(pso.belt.cull);
    pso.post.meter = gpu::create_compute_pso(device, read_spirv(directory / "shaders/meter_histogram.compute.spv"));
    panic_if(!pso.post.meter, "compute pipeline creation failed: meter_histogram");
    pipelines.emplace_back(pso.post.meter);
    // Depth-only shadow pass reuses the surface vertex shader with a slope bias.
    const auto shadow_vertex = read_spirv(directory / "shaders/surface.vertex.spv");
    pso.scene.shadow = gpu::create_graphics_pso(
        device, {.vertex_spirv = shadow_vertex,
                 .depth_format = Format::d32_float,
                 .rasterization = {.depth_bias_constant = 1, .depth_bias_slope = 1.5f}});
    panic_if(!pso.scene.shadow, "shadow pipeline creation failed");
    pipelines.emplace_back(pso.scene.shadow);
    // The bodies' depth pre-pass: the same vertex shader and view, so the scene pass matches it exactly.
    pso.scene.depth_prepass = gpu::create_graphics_pso(
        device, {.vertex_spirv = shadow_vertex, .depth_format = Format::d32_float});
    panic_if(!pso.scene.depth_prepass, "depth pre-pass pipeline creation failed");
    pipelines.emplace_back(pso.scene.depth_prepass);
}

} // namespace space::render
