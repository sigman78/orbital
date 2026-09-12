#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/panic.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace space::render {
namespace {

// Temporal anti-aliasing sub-pixel offsets, cycled per frame.
constexpr Float4 jitter_sequence[8] = {{0, -.166667f, 0, 0},      {-.25f, .166667f, 0, 0},  {.25f, -.388889f, 0, 0},
                                       {-.375f, -.055556f, 0, 0}, {.125f, .277778f, 0, 0},  {-.125f, -.277778f, 0, 0},
                                       {.375f, .055556f, 0, 0},   {-.4375f, .388889f, 0, 0}};
constexpr unsigned jitter_count = 8;

// Exposure adaptation, read by apply_metering and record_post_passes. The
// meter is centre weighted; adaptation is slight (about one stop either way)
// and asymmetric like the eye: quick to close down, slow to open up.
// Exposure gain ahead of the tone curves, read by build_frame. The base gain
// brings the Hill ACES fit, which sits about a stop under the Narkowicz
// approximation it replaced, back toward the earlier brightness. The per-curve
// trims were fitted on captures so the shadows of each curve match ACES (see
// docs/DECISIONS.md); switching curves then compares rendering intent rather
// than brightness.
namespace tone_curves {
inline constexpr float base_gain = 1.5f;
inline constexpr float exposure_trim[3] = {1.f, .37f, .75f}; // ACES filmic, AgX, Khronos PBR Neutral
} // namespace tone_curves

namespace exposure_meter {
inline constexpr unsigned interval = 16;                               // frames between readbacks
inline constexpr float min_luminance = 0.004f;                         // darker texels (space) do not vote
inline constexpr float key = 0.18f;                                    // middle gray
inline constexpr float brighten_seconds = 2.5f, darken_seconds = 0.6f; // time constants of the exposure change
inline constexpr float max_step_seconds = 1.f;                         // a stalled frame does not snap the adaptation
inline constexpr Range<float> adapted{0.6f, 1.8f};
} // namespace exposure_meter

// Temporal history invalidation, read by build_frame.
namespace history {
inline constexpr double max_jump = 10.0;       // camera translation that invalidates the history
inline constexpr double min_forward_dot = 0.7; // turn that invalidates the history
} // namespace history

// Shadow map placement, read by build_frame.
namespace shadow_placement {
inline constexpr double giant_distance = 100.0; // inside this the map follows the giant, else the nearer planet
inline constexpr float giant_half_size = 70.f, earth_half_size = 12.f, mars_half_size = 10.f;
} // namespace shadow_placement

// Sun disc and lens flare, read by build_frame.
namespace sun_flare {
inline constexpr float disc_radius = 3.0f;       // Frame.sun.w
inline constexpr float screen_size = 0.008f;     // Frame.screen_sun.w
inline constexpr float max_screen_offset = 1.3f; // beyond this the flare is off-screen
} // namespace sun_flare

// Belt motion and per-rock culling limits, read by write_cull_scratch.
namespace belt_culling {
inline constexpr double spin_rate = 0.0012, rock_spin_rate = 0.02; // radians per simulation second, barely visible
inline constexpr float shear_exponent =
    0.35f; // orbital rate falls with radius as r^-0.35: a hint of Kepler shear, not the real 1.5
namespace billboard {
inline constexpr float min_pixels = 0.06f; // smaller rocks are dropped
} // namespace billboard
} // namespace belt_culling

Float4 f4(Vec3f v, float w = 0) {
    return {v.x, v.y, v.z, w};
}

Float4 f4(Vec3d v, float w = 0) {
    return f4(to_float(v), w);
}

// The belt is tilted and compressed relative to the giant, and spins with time,
// each radial band at its own rate so the rings slowly shear past each other.
// The tilt is a rotation about X (cos .933, sin .36), so the belt plane normal
// is fixed; the surface shader uses it to cast the belt's shadow on the giant.
constexpr Vec3f belt_plane_normal{0.f, .933f, .36f};
struct BeltTilt {
    float y_scale = .7f, y_from_z = .36f, z_scale = .933f; // (x, y, z) -> (x, y * y_scale - z * y_from_z, z * z_scale)
};
constexpr BeltTilt belt_tilt{};
struct BeltTransform {
    float cos_spin[belt::radial_bands], sin_spin[belt::radial_bands];
};

BeltTransform belt_transform(const BeltDescription& description, float angle) {
    const float inner = float(description.inner_radius), outer = float(description.outer_radius);
    const float middle = (inner + outer) * .5f;
    BeltTransform transform{};
    for (unsigned band = 0; band < belt::radial_bands; band++) {
        const float radius = inner + (float(band) + .5f) / float(belt::radial_bands) * (outer - inner);
        const float band_angle = angle * std::pow(middle / radius, belt_culling::shear_exponent);
        transform.cos_spin[band] = std::cos(band_angle);
        transform.sin_spin[band] = std::sin(band_angle);
    }
    return transform;
}

void write_projection(FrameData& frame, const Camera& camera, float aspect) {
    const Vec3f right = to_float(camera.right()), up = to_float(camera.up()), forward = to_float(camera.forward());
    const float tan_half = float(std::tan(camera.vertical_fov * .5));
    const Mat4 view_projection = perspective_matrix(tan_half, aspect, targets::depth.min, targets::depth.max) *
                                 view_matrix(right, up, forward);
    std::memcpy(frame.view_projection, view_projection.m, sizeof frame.view_projection);
    frame.right_tan = f4(right, tan_half);
    frame.up_aspect = f4(up, aspect);
    frame.forward_exposure = f4(forward, 1);
}

// Light box looking from the sun at a camera-relative body, square and
// three half sizes deep either way.
LightFrame body_light_frame(Vec3d centre, Vec3d sun, float half_size) {
    const Vec3d forward = normalized(centre - sun), r = normalized(Vec3d{-forward.z, 0, forward.x});
    return {.centre = to_float(centre),
            .forward = to_float(forward),
            .right = to_float(r),
            .up = to_float(cross(r, forward)),
            .half_x = half_size,
            .half_y = half_size,
            .half_depth = half_size * 3};
}

// Light box fitted to the whole belt annulus: up follows the belt normal's
// component across the light, so the tilted annulus projects to a rectangle
// outer wide and outer * cos(tilt) tall.
LightFrame belt_light_frame(Vec3d giant, Vec3d sun, const BeltDescription& belt) {
    constexpr double margin = 2.0; // world units of slack around the belt's extent
    const double reach = belt.inner_radius + (belt.outer_radius - belt.inner_radius) * geometry::belt_outer_tail;
    const Vec3d forward = normalized(giant - sun);
    const Vec3d normal = to_double(belt_plane_normal);
    const double tilt = dot(normal, forward);
    Vec3d up = normal - forward * tilt;
    if (dot(up, up) < 1e-8)
        up = Vec3d{0, 1, 0} - forward * forward.y;
    up = normalized(up);
    const double extent = reach + belt.thickness + margin;
    return {.centre = to_float(giant),
            .forward = to_float(forward),
            .right = to_float(cross(up, forward)),
            .up = to_float(up),
            .half_x = float(extent),
            .half_y = float(reach * std::abs(tilt) + belt.thickness * 3 + margin),
            .half_depth = float(extent)};
}

// Orthographic projection of a light box, in double like the frame it comes from.
void write_light_projection(float* m, const LightFrame& light) {
    const Vec3d forward = to_double(light.forward), r = to_double(light.right), u = to_double(light.up);
    const Vec3d eye = to_double(light.centre) - forward * light.half_depth;
    const double depth_range = light.half_depth * 2.0;
    m[0] = float(r.x / light.half_x), m[4] = float(r.y / light.half_x), m[8] = float(r.z / light.half_x);
    m[12] = float(-dot(r, eye) / light.half_x);
    m[1] = float(-u.x / light.half_y), m[5] = float(-u.y / light.half_y), m[9] = float(-u.z / light.half_y);
    m[13] = float(dot(u, eye) / light.half_y);
    m[2] = float(forward.x / depth_range), m[6] = float(forward.y / depth_range),
    m[10] = float(forward.z / depth_range);
    m[14] = float(-dot(forward, eye) / depth_range);
    m[15] = 1;
}

} // namespace

void Renderer::Impl::read_gpu_timings() {
    if (!frame_index)
        return;
    const auto* t = reinterpret_cast<const std::uint64_t*>(timestamps.range.cpu);
    const float scale = float(gpu::get_device_caps(device).timestamp_period_ns * 1e-6);
    stats.gpu_ms = float(t[4] - t[0]) * scale;
    stats.shadow_ms = float(t[1] - t[0]) * scale;
    stats.surface_ms = float(t[2] - t[1]) * scale;
    stats.atmosphere_ms = float(t[3] - t[2]) * scale;
    stats.post_ms = float(t[4] - t[3]) * scale;
}

void Renderer::Impl::apply_metering() {
    if (!meter_pending)
        return;
    // Each meter texel holds log luminance, luminance and its centre weight.
    const auto* values = reinterpret_cast<const float*>(luminance_readback.range.cpu);
    float log_sum = 0, weight_sum = 0;
    for (unsigned i = 0; i < targets::meter_size * targets::meter_size; i++)
        if (values[i * 4 + 1] > exposure_meter::min_luminance) {
            log_sum += values[i * 4] * values[i * 4 + 2];
            weight_sum += values[i * 4 + 2];
        }
    const float target = weight_sum > 0
                             ? exposure_meter::adapted.clamp(exposure_meter::key / std::exp(log_sum / weight_sum))
                             : 1.f;
    const auto now = std::chrono::steady_clock::now();
    const float dt = meter_time == std::chrono::steady_clock::time_point{}
                         ? exposure_meter::max_step_seconds
                         : std::min(std::chrono::duration<float>(now - meter_time).count(),
                                    exposure_meter::max_step_seconds);
    meter_time = now;
    const float tau = target < adapted_exposure ? exposure_meter::darken_seconds : exposure_meter::brighten_seconds;
    adapted_exposure += (target - adapted_exposure) * (1 - std::exp(-dt / tau));
    meter_pending = false;
}

FrameData Renderer::Impl::build_frame(const FrameInput& input) {
    const Camera& camera = input.camera;
    FrameData frame{};
    write_projection(frame, camera, extent.aspect());
    const Float4 jitter = jitter_sequence[frame_index % jitter_count];
    frame.jitter = {jitter.x, jitter.y, previous_frame.jitter.x, previous_frame.jitter.y};
    for (unsigned column = 0; column < 4; column++) {
        frame.view_projection[column * 4] += 2 * frame.jitter.x / float(extent.width) *
                                             frame.view_projection[column * 4 + 3];
        frame.view_projection[column * 4 + 1] += 2 * frame.jitter.y / float(extent.height) *
                                                 frame.view_projection[column * 4 + 3];
    }
    std::memcpy(frame.previous_projection, previous_frame.view_projection, sizeof frame.previous_projection);
    frame.previous_camera_delta = f4(camera.position - previous_camera, history_valid ? 1.f : 0.f);
    frame.previous_forward = previous_frame.forward_exposure;
    const Vec3d previous_forward{previous_frame.forward_exposure.x, previous_frame.forward_exposure.y,
                                 previous_frame.forward_exposure.z};
    if (length(camera.position - previous_camera) > history::max_jump ||
        dot(camera.forward(), previous_forward) < history::min_forward_dot)
        frame.previous_camera_delta.w = 0;
    frame.camera_time = {0, 0, 0, float(input.time)};
    frame.forward_exposure.w = input.exposure * (input.auto_exposure ? adapted_exposure : 1) * tone_curves::base_gain *
                               tone_curves::exposure_trim[std::min(input.tone_curve, 2u)];
    frame.sun = f4(system.star.position - camera.position, sun_flare::disc_radius);
    const auto distance_to = [&](unsigned body) { return length(input.bodies[body].position - camera.position); };
    const bool giant_close = distance_to(giant_index) < shadow_placement::giant_distance;
    const bool mars_closer = distance_to(mars_index) < distance_to(earth_index);
    const unsigned shadow_body = giant_close ? giant_index : (mars_closer ? mars_index : earth_index);
    const float shadow_half_size = giant_close   ? shadow_placement::giant_half_size
                                   : mars_closer ? shadow_placement::mars_half_size
                                                 : shadow_placement::earth_half_size;
    const Vec3d sun_relative = system.star.position - camera.position;
    write_light_projection(
        frame.light_projection,
        body_light_frame(input.bodies[shadow_body].position - camera.position, sun_relative, shadow_half_size));
    const auto& ring = system.belts[0];
    // The belt transmittance map's box, and the slice span along the light
    // that the belt's thickness covers at the sun's tilt.
    const Vec3d giant_relative = input.bodies[giant_index].position - camera.position;
    const LightFrame belt_box = belt_light_frame(giant_relative, sun_relative, ring);
    write_light_projection(frame.belt_light_projection, belt_box);
    const double light_tilt = std::abs(dot(to_double(belt_box.forward), to_double(belt_plane_normal)));
    constexpr double slice_slack = 3.0; // the slices reach three half thicknesses: the flared Gaussian tails
    frame.belt_light = {belt_box.half_x, belt_box.half_y, float(targets::belt_light_map_size),
                        float(slice_slack * ring.thickness * .5 / std::max(light_tilt, .1))};
    frame.belt_ring = {float(ring.inner_radius), float(ring.outer_radius), float(ring.density), float(ring.thickness)};
    frame.belt_normal = f4(belt_plane_normal);
    frame.options = {float(extent.width), float(extent.height), input.high_quality ? 1.f : 0.f,
                     input.overlay ? 1.f : 0.f};
    for (unsigned i = 0; i < body_count; i++)
        frame.bodies[i] = f4(input.bodies[i].position - camera.position, float(input.bodies[i].radius));
    frame.scene = {float(body_count), float(giant_index), input.belt_light_map ? 1.f : 0.f,
                   input.belt_extinction ? 1.f : 0.f};
    frame.quality = {float(input.anti_aliasing), float(input.tone_curve), 0, 0};

    // Sun position in screen space for the lens flare, hidden when a body covers it.
    const Vec3d sun_direction = normalized(system.star.position - camera.position);
    const double forward = dot(sun_direction, camera.forward());
    const double view_depth = std::max(forward, .001);
    frame.screen_sun = {
        float(dot(sun_direction, camera.right()) / (view_depth * frame.right_tan.w * frame.up_aspect.w)),
        float(-dot(sun_direction, camera.up()) / (view_depth * frame.right_tan.w)), forward > 0 ? 1.f : 0.f,
        sun_flare::screen_size};
    for (const auto& body : input.bodies) {
        const Vec3d v = body.position - camera.position;
        const double along = dot(v, sun_direction);
        if (along > 0 && length(v - sun_direction * along) < body.radius)
            frame.screen_sun.z = 0;
    }
    if (std::abs(frame.screen_sun.x) > sun_flare::max_screen_offset ||
        std::abs(frame.screen_sun.y) > sun_flare::max_screen_offset)
        frame.screen_sun.z = 0;
    return frame;
}

// The body instances occupy the first slots of the per-frame instance list.
void Renderer::Impl::write_body_instances(const FrameInput& input, const FrameData& frame) {
    instances.clear();
    for (unsigned i = 0; i < body_count; i++) {
        const BodyClass body_class = system.bodies[i].body_class;
        // Moonlets are dark, reddish rock; the tint also seeds their texture offset.
        const Float4 tint = body_class == BodyClass::Moonlet ? Float4{.34f, .30f, .27f, 1} : Float4{1, 1, 1, 1};
        instances.push_back({frame.bodies[i],
                             {0, float(input.bodies[i].rotation_angle), float(system.bodies[i].axial_tilt),
                              float(surface_kind(body_class))},
                             tint});
    }
}

// Everything the GPU culling pass needs this frame, mirroring the CPU
// frustum test it replaced: plane normals are not unit length, so sphere
// support is scaled, and two pixels of padding cover temporal jitter and
// expanded tiny billboards.
void Renderer::Impl::write_cull_scratch(const FrameInput& input, const FrameData& frame, CullScratch& scratch,
                                        std::uint64_t instance_address) {
    const Camera& camera = input.camera;
    const float tan_y = frame.right_tan.w, tan_x = tan_y * frame.up_aspect.w;
    const BeltTransform transform = belt_transform(system.belts[0], float(input.time * belt_culling::spin_rate));
    CullParams& p = scratch.params;
    p = {};
    p.right = f4(to_float(camera.right()), tan_x);
    p.up = f4(to_float(camera.up()), tan_y);
    p.forward = f4(to_float(camera.forward()), tan_y * 4 / float(extent.height));
    p.view = {float(extent.height) / (2 * frame.right_tan.w), std::sqrt(1 + tan_x * tan_x),
              std::sqrt(1 + tan_y * tan_y), float(input.time * belt_culling::rock_spin_rate)};
    p.giant = f4(input.bodies[giant_index].position - camera.position);
    p.belt_tilt = {belt_tilt.y_scale, belt_tilt.y_from_z, belt_tilt.z_scale, 0};
    for (unsigned band = 0; band < belt::radial_bands; band++)
        p.band_spin[band] = {transform.cos_spin[band], transform.sin_spin[band], 0, 0};
    p.levels = {geometry::rock_level_thresholds[0], geometry::rock_level_thresholds[1],
                geometry::rock_level_thresholds[2], geometry::rock_level_thresholds[3]};
    p.billboard = {geometry::rock_level_thresholds[4], input.billboard_radius, belt_culling::billboard::min_pixels, 0};
    const unsigned tier_count = (input.high_quality ? high_quality : baseline_quality).belt_count;
    p.rock_limit = std::min(rock_count, belt_count_override ? belt_count_override : tier_count);
    p.body_count = body_count;
    p.light_in_count_pass = input.splat_light_twice ? 1u : 0u;
    for (unsigned group = 0; group < rock_group_count; group++) {
        p.index_counts[group] = rocks[group].index_count;
        p.first_indices[group] = rocks[group].first_index;
        p.vertex_offsets[group] = rocks[group].vertex_offset;
    }
    scratch.instances = instance_address;
    std::memset(scratch.counts, 0, sizeof scratch.counts);
    std::memset(scratch.cursors, 0, sizeof scratch.cursors);
}

// Statistics from the previous frame's culling, read back once its GPU work
// has completed. The triangle count is what those draws submitted.
void Renderer::Impl::read_cull_counts(const CullScratch& scratch) {
    unsigned rocks_visible = 0, triangles = 0, groups_drawn = 0;
    for (unsigned group = 0; group < rock_group_count; group++) {
        rocks_visible += scratch.counts[group];
        triangles += scratch.counts[group] * (scratch.params.index_counts[group] / 3);
        groups_drawn += scratch.counts[group] ? 1 : 0;
    }
    stats.rock_groups_drawn = groups_drawn; // equals scratch.draw_count once the prefix pass has run
    rocks_visible += scratch.counts[rock_group_count];
    triangles += scratch.counts[rock_group_count] * 2;
    stats.visible_asteroids = rocks_visible;
    stats.rock_triangles = triangles;
}

void Renderer::Impl::record_cull_passes(gpu::CommandBuffer* cmd, const CullRoot& root) {
    constexpr unsigned pass_count = 3, prefix_pass = 1;
    gpu::bind_pso(cmd, pso.cull);
    const unsigned groups = (rock_count + ORBITAL_CULL_THREADS - 1) / ORBITAL_CULL_THREADS;
    for (unsigned pass = 0; pass < pass_count; pass++) {
        CullRoot pass_root = root;
        pass_root.pass = pass;
        gpu::dispatch(cmd, pass_root, {pass == prefix_pass ? 1u : groups, 1, 1});
        const bool last = pass + 1 == pass_count;
        gpu::barrier(cmd, gpu::Stage::compute, gpu::Access::shader_write,
                     last ? gpu::Stage::indirect | gpu::Stage::vertex | gpu::Stage::fragment : gpu::Stage::compute,
                     last ? gpu::Access::indirect_read | gpu::Access::shader_read
                          : gpu::Access::shader_read | gpu::Access::shader_write);
    }
}

void Renderer::Impl::draw_mesh(gpu::CommandBuffer* cmd, Root& root, const GpuMesh& mesh, unsigned base,
                               unsigned instance_count) {
    root.base = base;
    root.vertices = mesh.vertices;
    stats.draw_calls++;
    gpu::draw_indexed(cmd, root, {reinterpret_cast<void*>(mesh.indices), std::uint64_t(mesh.index_count) * 4},
                      gpu::IndexType::uint32, mesh.index_count, instance_count);
    stats.triangles += mesh.index_count / 3 * instance_count;
}

void Renderer::Impl::fullscreen_pass(gpu::CommandBuffer* cmd, GpuImage& target, gpu::PSO* pipeline, Root root,
                                     bool preserve) {
    gpu::ColorAttachment attachment{.render_view = target.view,
                                    .load = preserve ? gpu::LoadOp::load : gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd, {.colors = {&attachment, 1}});
    gpu::bind_pso(cmd, pipeline);
    gpu::draw(cmd, root, 3);
    stats.draw_calls++;
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
}

// Belt transmittance map: splat every rock's coverage into its light-space
// column and depth slice, then blur in both directions.
void Renderer::Impl::record_belt_light_pass(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root,
                                            unsigned rock_limit) {
    // Coverage accumulates from zero in every slice, alpha included.
    gpu::ColorAttachment attachment{.render_view = belt_light.view, .load = gpu::LoadOp::clear, .clear = {0, 0, 0, 0}};
    gpu::begin_render_pass(cmd, {.colors = {&attachment, 1}});
    gpu::bind_pso(cmd, pso.belt_splat);
    gpu::draw(cmd, cull_root, 6, rock_limit);
    stats.draw_calls++;
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    root.mode = 0;
    fullscreen_pass(cmd, belt_light_blur, pso.belt_blur, root);
    root.mode = 1;
    fullscreen_pass(cmd, belt_light, pso.belt_blur, root);
}

void Renderer::Impl::record_shadow_pass(gpu::CommandBuffer* cmd, Root root) {
    root.mode = std::uint32_t(SurfaceMode::shadow);
    gpu::begin_render_pass(cmd, {.depth = {.render_view = shadow_map.view, .load = gpu::LoadOp::clear}});
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, pso.shadow);
    const unsigned triangles_before = stats.triangles;
    for (unsigned i = 0; i < body_count; i++)
        draw_mesh(cmd, root, body_mesh(i, 2), i, 1);
    stats.triangles = triangles_before; // shadow geometry is not counted in the frame statistic
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
}

void Renderer::Impl::record_scene_pass(gpu::CommandBuffer* cmd, Root root, const FrameInput& input,
                                       const FrameData& frame, std::uint64_t args_address) {
    root.mode = std::uint32_t(SurfaceMode::opaque);
    gpu::ColorAttachment color{.render_view = hdr.view, .load = gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd,
                           {.colors = {&color, 1}, .depth = {.render_view = depth.view, .load = gpu::LoadOp::clear}});
    gpu::bind_pso(cmd, pso.background);
    gpu::draw(cmd, root, 3);
    stats.draw_calls++;
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, pso.opaque);
    for (unsigned i = 0; i < body_count; i++) {
        const float distance = float(length(input.bodies[i].position - input.camera.position));
        const float projected = float(input.bodies[i].radius) * float(extent.height) / (distance * frame.right_tan.w);
        draw_mesh(cmd, root, body_mesh(i, geometry::select_lod(projected, 2)), i, 1);
    }
    // Rocks: one multi-draw over the pooled mesh; the culling pass wrote every
    // group's count, base and slice into the argument array.
    root.base = 0;
    root.vertices = rock_pool.vertices;
    gpu::draw_indexed_indirect_count(
        cmd, root, {reinterpret_cast<void*>(rock_pool.indices), std::uint64_t(rock_pool.index_count) * 4},
        gpu::IndexType::uint32, {reinterpret_cast<void*>(args_address), sizeof(DrawArgs) * rock_group_count},
        {reinterpret_cast<void*>(args_address - offsetof(CullScratch, args) + offsetof(CullScratch, draw_count)),
         sizeof(std::uint32_t)},
        rock_group_count, sizeof(DrawArgs));
    stats.draw_calls++;
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = false});
    gpu::bind_pso(cmd, pso.cloud);
    root.mode = std::uint32_t(SurfaceMode::billboard);
    gpu::draw_indirect(cmd, root,
                       {reinterpret_cast<void*>(args_address + rock_group_count * sizeof(DrawArgs)), sizeof(DrawArgs)},
                       1, sizeof(DrawArgs));
    stats.draw_calls++;
    stats.triangles += stats.rock_triangles;
    root.mode = std::uint32_t(SurfaceMode::cloud);
    draw_mesh(cmd, root, spheres[geometry::lod_count - 1], earth_index, 1);
    gpu::end_render_pass(cmd);
}

void Renderer::Impl::record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view,
                                        bool spatial_aa) {
    const unsigned history_write = frame_index % 2;
    root.mode = std::uint32_t(PostMode::tonemap);
    fullscreen_pass(cmd, history[history_write], pso.temporal, root);
    root.mode = std::uint32_t(PostMode::bloom_a);
    fullscreen_pass(cmd, bloom_a, pso.bloom, root);
    root.mode = std::uint32_t(PostMode::bloom_b);
    fullscreen_pass(cmd, bloom_b, pso.bloom, root);
    // Tone map into the final image, or through an intermediate when FXAA follows.
    root.mode = std::uint32_t(PostMode::tonemap);
    fullscreen_pass(cmd, spatial_aa ? ldr : final_image, pso.post, root);
    if (spatial_aa) {
        root.mode = std::uint32_t(PostMode::fxaa);
        fullscreen_pass(cmd, final_image, pso.fxaa, root);
    }
    if (frame_index % exposure_meter::interval == 0) {
        root.mode = std::uint32_t(PostMode::meter);
        fullscreen_pass(cmd, luminance, pso.meter, root);
        gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::transfer,
                     gpu::Access::transfer_read);
        gpu::copy_texture_to_memory(cmd, luminance.texture, gpu::gpu_range(luminance_readback));
        gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
        meter_pending = true;
    }
    gpu::ColorAttachment color{.render_view = swapchain_view, .load = gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd, {.colors = {&color, 1}});
    root.mode = std::uint32_t(PostMode::present);
    gpu::bind_pso(cmd, pso.present);
    gpu::draw(cmd, root, 3);
    gpu::end_render_pass(cmd);
}

bool Renderer::draw(const FrameInput& input) {
    const auto start = std::chrono::steady_clock::now();
    auto& s = *impl_;
    ORBITAL_ASSERT(input.bodies.size() == s.body_count);
    gpu::wait_timeline({s.timeline, s.serial});
    s.read_gpu_timings();
    s.apply_metering();
    const auto drawable = gpu::get_drawable_extent(s.device);
    if (!drawable.x || !drawable.y)
        return false;
    s.resize({drawable.x, drawable.y});
    const auto swap = gpu::acquire(s.device);
    if (!swap.render_view)
        return false;
    const auto prepare_start = std::chrono::steady_clock::now();

    const unsigned history_write = s.frame_index % 2;
    s.bind(Slot::history_a, s.history[history_write]);
    s.bind(Slot::history_b, s.history[1 - history_write]);
    const FrameData frame = s.build_frame(input);
    s.write_body_instances(input, frame);
    s.stats.triangles = 0;
    s.stats.draw_calls = 0;

    // Per-frame constants, culling scratch and instances live in the dynamic
    // half of the static heap; the previous frame's culling counts are read
    // before the scratch is reset.
    const auto frame_address = reinterpret_cast<std::uint64_t>(s.data.range.gpu) + heap_layout.dynamic_offset;
    auto* dynamic = s.data.range.cpu + heap_layout.dynamic_offset;
    auto* scratch = reinterpret_cast<CullScratch*>(dynamic + heap_layout.cull_offset);
    s.read_cull_counts(*scratch);
    std::memcpy(dynamic, &frame, sizeof frame);
    s.write_cull_scratch(input, frame, *scratch, frame_address + heap_layout.instance_offset);
    std::memcpy(dynamic + heap_layout.instance_offset, s.instances.data(), s.instances.size() * sizeof(Instance));
    Root root{.frame = frame_address,
              .vertices = 0,
              .instances = frame_address + heap_layout.instance_offset,
              .base = 0,
              .mode = 0};
    const CullRoot cull_root{.frame = frame_address,
                             .rocks = s.rock_data,
                             .scratch = frame_address + heap_layout.cull_offset,
                             .pass = 0,
                             .unused = 0};
    const std::uint64_t args_address = frame_address + heap_layout.cull_offset + offsetof(CullScratch, args);

    auto* cmd = gpu::begin_commands(s.device);
    const auto stamp = [&](unsigned index) {
        ORBITAL_ASSERT(index < targets::timestamp_count);
        gpu::write_timestamp(cmd,
                             reinterpret_cast<gpu::uint64*>(s.timestamps.range.gpu + index * sizeof(std::uint64_t)));
    };
    stamp(0);
    gpu::set_texture_descriptor_heap(cmd, gpu::gpu_range(s.texture_descriptors));
    gpu::set_sampler_descriptor_heap(cmd, gpu::gpu_range(s.sampler_descriptors));
    gpu::barrier(cmd, gpu::Stage::all_commands,
                 gpu::Access::shader_read | gpu::Access::color_write | gpu::Access::depth_stencil_write,
                 gpu::Stage::all_commands,
                 gpu::Access::color_write | gpu::Access::depth_stencil_write | gpu::Access::shader_read);
    s.record_cull_passes(cmd, cull_root);
    s.record_shadow_pass(cmd, root);
    if (input.belt_light_map) {
        // Only the size-tail rocks splat; their ids are sorted, so the tier limit is a prefix.
        const auto tail_end = std::upper_bound(s.rock_tail_ids.begin(), s.rock_tail_ids.end(),
                                               scratch->params.rock_limit - 1);
        const CullRoot splat_root{.frame = frame_address,
                                  .rocks = s.rock_tail_data,
                                  .scratch = cull_root.scratch,
                                  .pass = unsigned(tail_end - s.rock_tail_ids.begin()),
                                  .unused = 0};
        s.record_belt_light_pass(cmd, splat_root, root, splat_root.pass);
    }
    stamp(1);
    s.record_scene_pass(cmd, root, input, frame, args_address);
    stamp(2);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::color_output,
                 gpu::Access::color_read | gpu::Access::color_write);
    // The atmosphere passes and the temporal pass read the scene depth.
    gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    // Atmosphere is composited per body; the depth clamp orders them against
    // geometry, and Earth's goes last so it stays on top where shells overlap.
    root.mode = 0;
    for (unsigned body : {s.giant_index, s.mars_index, s.earth_index}) {
        root.base = body;
        s.fullscreen_pass(cmd, s.hdr, s.pso.atmosphere, root, true);
    }
    stamp(3);
    s.record_post_passes(cmd, root, swap.render_view, input.anti_aliasing >= 2);
    stamp(4);
    s.stats.prepare_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - prepare_start).count();
    gpu::submit_and_present(s.device, {cmd}, {s.timeline, ++s.serial});

    s.frame_index++;
    s.previous_frame = frame;
    s.previous_camera = input.camera.position;
    s.history_valid = true;
    s.stats.frame_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}

bool Renderer::capture(const std::filesystem::path& path) {
    auto& s = *impl_;
    if (s.extent.empty())
        return false;
    gpu::wait_timeline({s.timeline, s.serial});
    auto readback = gpu::create_gpu_heap(s.device, std::uint64_t(s.extent.width) * s.extent.height * 4,
                                         gpu::MemoryType::readback);
    if (!readback.range.cpu) {
        log::error("screenshot readback allocation failed");
        return false;
    }
    auto* cmd = gpu::begin_commands(s.device);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::transfer,
                 gpu::Access::transfer_read);
    gpu::copy_texture_to_memory(cmd, s.final_image.texture, gpu::gpu_range(readback));
    gpu::barrier(cmd, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
    gpu::submit({cmd}, {s.timeline, ++s.serial});
    gpu::wait_timeline({s.timeline, s.serial});
    const auto* rgba = reinterpret_cast<const std::uint8_t*>(readback.range.cpu);
    Bytes rgb(std::size_t(s.extent.width) * s.extent.height * 3);
    for (std::size_t i = 0; i < std::size_t(s.extent.width) * s.extent.height; i++)
        for (unsigned channel = 0; channel < 3; channel++)
            rgb[i * 3 + channel] = rgba[i * 4 + channel];
    gpu::destroy_gpu_heap(readback);
    return assets::save_png(path, s.extent, 3, rgb);
}

} // namespace space::render
