#include "render/renderer_impl.hpp"

#include "core/log.hpp"
#include "core/timing.hpp"

#include "core/panic.hpp"
#include "imgui.h"

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

// The belt fades from its near paths (dust march, splats) to the baked disc
// between these distances outside the belt, in units of the dust's distance
// ramp (DUST_FAR_DISTANCE slab half-heights; the ramp saturates at 1), so the
// handover happens once both paths agree on brightness. The slab half height
// matches DUST_SLAB in dust.slang.
namespace belt_lod {
inline constexpr double fade_start = 1.0, fade_end = 2.0;
inline constexpr double slab_half_heights = 3.5, ramp_half_heights = 6.0;
} // namespace belt_lod

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
    const auto caps = gpu::get_device_caps(device);
    const float scale = float(caps.timestamp_period_ns * 1e-6);
    const auto elapsed = [&](unsigned begin, unsigned end) {
        return float(timestamp_ticks(t[begin], t[end], caps.timestamp_valid_bits)) * scale;
    };
    stats.gpu_ms = elapsed(0, 4);
    stats.shadow_ms = elapsed(0, 1);
    stats.surface_ms = elapsed(1, 2);
    stats.atmosphere_ms = elapsed(2, 3);
    stats.post_ms = elapsed(3, 4);
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
    // The far-belt maps' frame: an orthographic box over the belt plane, and
    // the weight that fades the near paths (dust march, splats) into the baked
    // disc as the camera leaves the belt, measured in belt widths past the slab.
    {
        const Vec3d normal = to_double(belt_plane_normal);
        Vec3d right = cross(normal, Vec3d{0, 0, 1});
        if (dot(right, right) < 1e-8)
            right = cross(normal, Vec3d{1, 0, 0});
        right = normalized(right);
        const Vec3d up = cross(normal, right);
        const double reach = ring.inner_radius + (ring.outer_radius - ring.inner_radius) * geometry::belt_outer_tail;
        const double half_extent = reach + 1.0;
        const LightFrame disc_box{.centre = to_float(giant_relative),
                                  .forward = to_float(normal * -1.0),
                                  .right = to_float(right),
                                  .up = to_float(up),
                                  .half_x = float(half_extent),
                                  .half_y = float(half_extent),
                                  .half_depth = float(ring.thickness * 4 + 1.0)};
        write_light_projection(frame.belt_disc_projection, disc_box);
        frame.belt_disc_right = f4(to_float(right), float(half_extent));
        frame.belt_disc_up = f4(to_float(up), float(half_extent));
        const Vec3d q = giant_relative * -1.0; // the camera in the giant's frame
        const double h = dot(q, normal);
        const double r = length(q - normal * h);
        const double slab = ring.thickness * .5 * 1.4 * belt_lod::slab_half_heights;
        const double outside = std::max(std::max(std::abs(h) - slab, r - reach), 0.0) /
                               (slab * belt_lod::ramp_half_heights);
        const double scale = std::max(double(input.belt_lod_scale), .05);
        const double u = std::clamp(
            (outside / scale - belt_lod::fade_start) / (belt_lod::fade_end - belt_lod::fade_start), 0.0, 1.0);
        const float weight = input.belt_disc ? float(u * u * (3 - 2 * u)) : 0.f;
        frame.belt_disc = {float(targets::belt_disc_map_size), weight, float(targets::belt_disc_rock_map_size),
                           input.belt_disc ? 1.f : 0.f};
        stats.belt_lod = weight;
    }
    frame.belt_normal = f4(belt_plane_normal);
    frame.options = {float(extent.width), float(extent.height), input.high_quality ? 1.f : 0.f,
                     input.overlay ? 1.f : 0.f};
    for (unsigned i = 0; i < body_count; i++)
        frame.bodies[i] = f4(input.bodies[i].position - camera.position, float(input.bodies[i].radius));
    frame.scene = {float(body_count), float(giant_index), input.belt_light_map ? 1.f : 0.f,
                   input.belt_extinction ? 1.f : 0.f};
    frame.quality = {input.temporal_aa ? 1.f : 0.f, float(input.tone_curve), input.belt_dust ? 1.f : 0.f, 0};
    frame.dust = {input.dust_density, input.dust_brightness, input.dust_far, input.dust_saturation};
    frame.dust_tint = {input.dust_tint[0], input.dust_tint[1], input.dust_tint[2], 0};
    frame.earth = {input.ocean_roughness, input.glint_intensity, input.cloud_shadow, input.cloud_shadow_softness};
    frame.earth_more = {input.sea_patchiness, input.cloud_opacity, 0, 0};

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
    p.billboard = {geometry::rock_level_thresholds[4], input.billboard_radius, belt_culling::billboard::min_pixels,
                   frame.belt_disc.y};
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

// Far-belt maps: the sunlight over the belt plane every few frames, and the
// rocks' coverage splatted every few dozen frames, both only while the far
// tier is in use (and once before it first shows).
void Renderer::Impl::record_belt_disc_bakes(gpu::CommandBuffer* cmd, const CullRoot& cull_root, Root root,
                                            unsigned rock_limit, float far_weight) {
    if (far_weight <= 0)
        return;
    const bool bake_light = !belt_disc_baked || frame_index % targets::belt_disc_light_interval == 0;
    const bool bake_rocks = !belt_disc_baked || frame_index % targets::belt_disc_rock_interval == 0;
    if (bake_light) {
        root.mode = 0;
        fullscreen_pass(cmd, belt_disc_light, pso.belt_disc, root);
    }
    if (bake_rocks) {
        gpu::ColorAttachment attachment{
            .render_view = belt_disc_rocks.view, .load = gpu::LoadOp::clear, .clear = {0, 0, 0, 0}};
        gpu::begin_render_pass(cmd, {.colors = {&attachment, 1}});
        gpu::bind_pso(cmd, pso.belt_disc_splat);
        gpu::draw(cmd, cull_root, 6, rock_limit);
        stats.draw_calls++;
        gpu::end_render_pass(cmd);
        gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                     gpu::Access::shader_read);
    }
    belt_disc_baked = true;
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
    stats.triangles += stats.rock_triangles;
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = false});
    gpu::bind_pso(cmd, pso.cloud);
    root.mode = std::uint32_t(SurfaceMode::billboard);
    gpu::draw_indirect(cmd, root,
                       {reinterpret_cast<void*>(args_address + rock_group_count * sizeof(DrawArgs)), sizeof(DrawArgs)},
                       1, sizeof(DrawArgs));
    stats.draw_calls++;
    root.mode = std::uint32_t(SurfaceMode::cloud);
    draw_mesh(cmd, root, spheres[geometry::lod_count - 1], earth_index, 1);
    gpu::end_render_pass(cmd);
}

// After the atmosphere has read the scene depth, the rock splats write their
// own depth and mark their pixels. The temporal pass then reprojects them
// exactly, as rocks at their distance rather than as sky, and keeps their
// history unclipped: a sub-pixel feature in motion never survives the
// neighbourhood clip, which faded the splats whenever the camera translated.
// The atmosphere must not see this depth, since a splat covers only a
// fraction of its pixels.
void Renderer::Impl::record_splat_mask_pass(gpu::CommandBuffer* cmd, Root root, std::uint64_t args_address) {
    root.mode = std::uint32_t(SurfaceMode::splat_mask);
    root.base = 0;
    gpu::ColorAttachment mask{.render_view = splat_mask.view, .load = gpu::LoadOp::clear, .clear = {0, 0, 0, 0}};
    gpu::begin_render_pass(cmd,
                           {.colors = {&mask, 1}, .depth = {.render_view = depth.view, .load = gpu::LoadOp::load}});
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, pso.splat_mask);
    gpu::draw_indirect(cmd, root,
                       {reinterpret_cast<void*>(args_address + rock_group_count * sizeof(DrawArgs)), sizeof(DrawArgs)},
                       1, sizeof(DrawArgs));
    stats.draw_calls++;
    gpu::end_render_pass(cmd);
}

void Renderer::Impl::record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view,
                                        unsigned spatial_aa, const ImDrawData* ui, std::uint8_t* ui_cpu,
                                        std::uint64_t ui_gpu) {
    const unsigned history_write = frame_index % 2;
    root.mode = std::uint32_t(PostMode::tonemap);
    fullscreen_pass(cmd, history[history_write], pso.temporal, root);
    root.mode = std::uint32_t(PostMode::bloom_a);
    fullscreen_pass(cmd, bloom_a, pso.bloom, root);
    root.mode = std::uint32_t(PostMode::bloom_b);
    fullscreen_pass(cmd, bloom_b, pso.bloom, root);
    // Tone map into the final image, or through an intermediate when a spatial pass follows.
    root.mode = std::uint32_t(PostMode::tonemap);
    fullscreen_pass(cmd, spatial_aa ? ldr : final_image, pso.post, root);
    if (spatial_aa == 1) {
        root.mode = std::uint32_t(PostMode::fxaa);
        fullscreen_pass(cmd, final_image, pso.fxaa, root);
    } else if (spatial_aa == 2) {
        // SMAA: edges, blending weights, neighbourhood blend (modes 0, 1, 2 of smaa.slang).
        root.mode = 0;
        fullscreen_pass(cmd, smaa_edges, pso.smaa_edges, root);
        root.mode = 1;
        fullscreen_pass(cmd, smaa_weights, pso.smaa_weights, root);
        root.mode = 2;
        fullscreen_pass(cmd, final_image, pso.smaa_blend, root);
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
    record_ui(cmd, ui, ui_cpu, ui_gpu);
    gpu::end_render_pass(cmd);
}

// Dear ImGui draw lists into the swapchain pass: vertices repacked into the
// overlay region of the dynamic heap, one indexed draw per command with its
// clip rectangle as the scissor.
void Renderer::Impl::record_ui(gpu::CommandBuffer* cmd, const ImDrawData* ui, std::uint8_t* cpu, std::uint64_t gpu) {
    if (!ui || ui->CmdListsCount == 0 || ui->TotalVtxCount == 0)
        return;
    const std::uint64_t vertex_bytes = std::uint64_t(ui->TotalVtxCount) * sizeof(UiVertex);
    const std::uint64_t index_bytes = std::uint64_t(ui->TotalIdxCount) * sizeof(ImDrawIdx);
    if (vertex_bytes + index_bytes > heap_layout.ui_bytes) {
        if (!ui_overflow_logged)
            log::warn("Overlay draw data ({} vertices) exceeds its {} MiB region; skipped", ui->TotalVtxCount,
                      heap_layout.ui_bytes >> 20);
        ui_overflow_logged = true;
        return;
    }
    auto* vertices = reinterpret_cast<UiVertex*>(cpu);
    auto* indices = reinterpret_cast<ImDrawIdx*>(cpu + vertex_bytes);
    const std::uint64_t index_address = gpu + vertex_bytes;
    gpu::bind_pso(cmd, pso.ui);
    UiRoot ui_root{.scale = {2.f / ui->DisplaySize.x, 2.f / ui->DisplaySize.y},
                   .translate = {},
                   .vertices = gpu,
                   .texture = std::uint32_t(Slot::ui_font),
                   .unused = 0};
    ui_root.translate[0] = -1 - ui->DisplayPos.x * ui_root.scale[0];
    ui_root.translate[1] = -1 - ui->DisplayPos.y * ui_root.scale[1];
    unsigned vertex_base = 0, index_base = 0;
    for (int n = 0; n < ui->CmdListsCount; n++) {
        const ImDrawList* list = ui->CmdLists[n];
        for (int i = 0; i < list->VtxBuffer.Size; i++) {
            const ImDrawVert& v = list->VtxBuffer[i];
            vertices[vertex_base + i] = {{v.pos.x, v.pos.y, v.uv.x, v.uv.y}, {v.col, 0, 0, 0}};
        }
        std::memcpy(indices + index_base, list->IdxBuffer.Data, std::size_t(list->IdxBuffer.Size) * sizeof(ImDrawIdx));
        for (const ImDrawCmd& draw : list->CmdBuffer) {
            if (draw.UserCallback || draw.ElemCount == 0)
                continue;
            const float x0 = std::max(draw.ClipRect.x - ui->DisplayPos.x, 0.f);
            const float y0 = std::max(draw.ClipRect.y - ui->DisplayPos.y, 0.f);
            const float x1 = std::min(draw.ClipRect.z - ui->DisplayPos.x, float(extent.width));
            const float y1 = std::min(draw.ClipRect.w - ui->DisplayPos.y, float(extent.height));
            if (x1 <= x0 || y1 <= y0)
                continue;
            gpu::set_scissor(cmd, {.x = std::int32_t(x0),
                                   .y = std::int32_t(y0),
                                   .width = std::uint32_t(x1 - x0),
                                   .height = std::uint32_t(y1 - y0)});
            ui_root.vertices = gpu + std::uint64_t(vertex_base + draw.VtxOffset) * sizeof(UiVertex);
            gpu::draw_indexed(cmd, gpu::ByteSpan(&ui_root, sizeof ui_root),
                              {reinterpret_cast<void*>(index_address +
                                                       std::uint64_t(index_base + draw.IdxOffset) * sizeof(ImDrawIdx)),
                               std::uint64_t(draw.ElemCount) * sizeof(ImDrawIdx)},
                              gpu::IndexType::uint16, draw.ElemCount);
            stats.draw_calls++;
        }
        vertex_base += unsigned(list->VtxBuffer.Size);
        index_base += unsigned(list->IdxBuffer.Size);
    }
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
    // The presentation mode the driver granted, once per change: what a frame-rate cap is made of.
    if (const auto info = gpu::get_swapchain_info(s.device);
        info.present_mode != s.logged_swapchain.present_mode || info.image_count != s.logged_swapchain.image_count) {
        s.logged_swapchain = info;
        log::info("Presentation: {} with {} swapchain images",
                  info.present_mode == gpu::PresentMode::fifo      ? "FIFO (vsync)"
                  : info.present_mode == gpu::PresentMode::mailbox ? "mailbox"
                                                                   : "immediate",
                  info.image_count);
    }
    if (swap.extent.x != s.extent.width || swap.extent.y != s.extent.height)
        log::warn("Swapchain {}x{} does not match the frame targets {}x{}", swap.extent.x, swap.extent.y,
                  s.extent.width, s.extent.height);
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
    {
        const CullRoot bake_root{.frame = frame_address,
                                 .rocks = s.rock_data,
                                 .scratch = cull_root.scratch,
                                 .pass = scratch->params.rock_limit,
                                 .unused = 0};
        s.record_belt_disc_bakes(cmd, bake_root, root, scratch->params.rock_limit, frame.belt_disc.y);
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
    // Belt dust scatters over everything the belt lies in front of, after the atmospheres.
    // Belt dust and the far-belt disc: the near march at half resolution, then
    // one composite that mixes it with the far tier by the LOD weight.
    const bool near_dust = input.belt_dust && frame.belt_disc.y < 1;
    if (near_dust) {
        root.mode = 0;
        s.fullscreen_pass(cmd, s.belt_dust, s.pso.belt_dust, root);
    }
    if (near_dust || frame.belt_disc.y > 0) {
        root.mode = 1;
        s.fullscreen_pass(cmd, s.hdr, s.pso.belt_dust_blend, root, true);
    }
    if (input.temporal_aa) {
        gpu::barrier(cmd, gpu::Stage::fragment, gpu::Access::shader_read, gpu::Stage::depth_stencil_tests,
                     gpu::Access::depth_stencil_write);
        s.record_splat_mask_pass(cmd, root, args_address);
        gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write, gpu::Stage::fragment,
                     gpu::Access::shader_read);
        gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                     gpu::Access::shader_read);
    }
    stamp(3);
    s.record_post_passes(cmd, root, swap.render_view, input.spatial_aa, input.ui, dynamic + heap_layout.ui_offset(),
                         frame_address + heap_layout.ui_offset());
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

void Renderer::set_vsync(bool vsync) {
    gpu::set_vsync(impl_->device, vsync);
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
