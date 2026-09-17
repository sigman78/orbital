#include "render/frame_calculations.hpp"
#include "scene/geometry.hpp"
#include "scene/system.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace space::render {
namespace {
// The belt fades from its near paths (dust march, splats) to the baked disc
// between these distances outside the belt, in units of the dust's distance
// ramp (DUST_FAR_DISTANCE slab half-heights; the ramp saturates at 1), so the
// handover happens once both paths agree on brightness. The slab half height
// matches DUST_SLAB in dust.slang.
namespace belt_lod {
inline constexpr double fade_start = 1.0, fade_end = 2.0;
inline constexpr double slab_half_heights = 3.5, ramp_half_heights = 6.0;
} // namespace belt_lod

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

} // namespace
// Light box looking from the sun at a camera-relative body, square and
// three half sizes deep either way.
geometry::Frustum view_frustum(const CameraView& camera, float tan_x, float tan_y) {
    const Vec3f forward = to_float(camera.forward), right = to_float(camera.right), up = to_float(camera.up);
    // Each side plane's inward normal: the forward axis leaned against the side by its tangent.
    const auto side = [&](Vec3f axis, float tan) { return normalized(forward * tan - axis); };
    geometry::Frustum frustum;
    frustum.planes[0] = {forward, 0};
    frustum.planes[1] = {side(right, tan_x), 0};
    frustum.planes[2] = {side(right * -1.f, tan_x), 0};
    frustum.planes[3] = {side(up, tan_y), 0};
    frustum.planes[4] = {side(up * -1.f, tan_y), 0};
    frustum.planes[5] = {forward, 0}; // the far plane is not tested
    return frustum;
}

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
Mat4 light_projection(const LightFrame& light) {
    Mat4 result{};
    for (float& value : result.m)
        value = 0;
    float* m = result.m;
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
    return result;
}

LightFrame prepare_body_light(Vec3d earth, Vec3d giant, Vec3d desert, Vec3d sun) {
    const bool giant_close = length(giant) < shadow_placement::giant_distance;
    const bool mars_closer = length(desert) < length(earth);
    const Vec3d centre = giant_close ? giant : (mars_closer ? desert : earth);
    const float half_size = giant_close   ? shadow_placement::giant_half_size
                            : mars_closer ? shadow_placement::mars_half_size
                                          : shadow_placement::earth_half_size;
    return body_light_frame(centre, sun, half_size);
}
PreparedBelt prepare_belt(const BeltDescription& ring, Vec3d giant_relative, Vec3d sun_relative, bool disc_enabled,
                          float lod_scale) {
    const LightFrame belt_box = belt_light_frame(giant_relative, sun_relative, ring);
    PreparedBelt result;
    result.light = belt_box;
    const double light_tilt = std::abs(dot(to_double(belt_box.forward), to_double(belt_plane_normal)));
    constexpr double slice_slack = 3.0; // the slices reach three half thicknesses: the flared Gaussian tails
    result.light_slice_depth = float(slice_slack * ring.thickness * .5 / std::max(light_tilt, .1));
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
        result.disc = disc_box;
        const Vec3d q = giant_relative * -1.0; // the camera in the giant's frame
        const double h = dot(q, normal);
        const double r = length(q - normal * h);
        const double slab = ring.thickness * .5 * 1.4 * belt_lod::slab_half_heights;
        const double outside = std::max(std::max(std::abs(h) - slab, r - reach), 0.0) /
                               (slab * belt_lod::ramp_half_heights);
        const double scale = std::max(double(lod_scale), .05);
        const double u = std::clamp(
            (outside / scale - belt_lod::fade_start) / (belt_lod::fade_end - belt_lod::fade_start), 0.0, 1.0);
        const float weight = disc_enabled ? float(u * u * (3 - 2 * u)) : 0.f;
        result.disc_weight = weight;
    }
    return result;
}
ScreenSun project_sun(const CameraView& camera, Vec3d sun, std::span<const BodyState> bodies, float tan_half_fov,
                      float aspect) {
    // Sun position in screen space for the lens flare, hidden when a body covers it.
    const Vec3d sun_direction = normalized(sun - camera.position);
    const double forward = dot(sun_direction, camera.forward);
    const double view_depth = std::max(forward, .001);
    ScreenSun result = {float(dot(sun_direction, camera.right) / (view_depth * tan_half_fov * aspect)),
                        float(-dot(sun_direction, camera.up) / (view_depth * tan_half_fov)), forward > 0};
    for (const auto& body : bodies) {
        const Vec3d v = body.position - camera.position;
        const double along = dot(v, sun_direction);
        if (along > 0 && length(v - sun_direction * along) < body.radius)
            result.visible = result.lit = false;
    }
    // The flare stack's ghosts stay while the sun is just outside the frame (lens.slang fades them by 2.1).
    constexpr float offscreen_reach = 2.2f;
    if (std::abs(result.x) > offscreen_reach || std::abs(result.y) > offscreen_reach)
        result.visible = false;
    return result;
}

PreparedCamera prepare_camera(const CameraView& camera, const CameraHistory& previous, Extent2D extent,
                              unsigned jitter_phase, bool temporal_aa, float near_plane, float far_plane,
                              unsigned lattice_cell_size) {
    constexpr float jitter[8][2] = {{0, -.166667f},    {-.25f, .166667f},   {.25f, -.388889f}, {-.375f, -.055556f},
                                    {.125f, .277778f}, {-.125f, -.277778f}, {.375f, .055556f}, {-.4375f, .388889f}};
    PreparedCamera result;
    result.right = to_float(camera.right);
    result.up = to_float(camera.up);
    result.forward = to_float(camera.forward);
    result.tan_half_fov = float(std::tan(camera.vertical_fov * .5));
    result.aspect = extent.aspect();
    result.projection = perspective_matrix_reversed(result.tan_half_fov, result.aspect, near_plane, far_plane) *
                        view_matrix(result.right, result.up, result.forward);
    result.jitter_x = temporal_aa ? jitter[jitter_phase % 8][0] : 0;
    result.jitter_y = temporal_aa ? jitter[jitter_phase % 8][1] : 0;
    for (unsigned column = 0; column < 4; ++column) {
        result.projection.m[column * 4] += 2 * result.jitter_x / float(extent.width) *
                                           result.projection.m[column * 4 + 3];
        result.projection.m[column * 4 + 1] += 2 * result.jitter_y / float(extent.height) *
                                               result.projection.m[column * 4 + 3];
    }
    result.delta = camera.position - previous.position;
    result.history_valid = previous.valid;
    if (temporal_aa != previous.temporal_aa || camera.vertical_fov != previous.vertical_fov ||
        camera.cut_serial != previous.cut_serial || length(result.delta) > history::max_jump ||
        dot(camera.forward, previous.forward) < history::min_forward_dot)
        result.history_valid = false;
    const double cell = lattice_cell_size;
    const Vec3d index{std::floor(camera.position.x / cell), std::floor(camera.position.y / cell),
                      std::floor(camera.position.z / cell)};
    result.lattice_offset = {float(camera.position.x - index.x * cell), float(camera.position.y - index.y * cell),
                             float(camera.position.z - index.z * cell)};
    result.lattice_cell = to_float(index);
    result.moving = length(result.delta) > 1e-9;
    return result;
}

MeterReading meter_exposure(std::span<const std::uint32_t> bins, std::uint32_t peak_bits, const ToneSettings& tone) {
    MeterReading reading;
    const auto luminance_of = [](std::size_t bin) {
        return std::exp2(float(ORBITAL_METER_LOG_MIN) +
                         (float(bin) + .5f) * float(ORBITAL_METER_LOG_RANGE) / float(ORBITAL_METER_BINS));
    };
    float total = 0;
    for (const auto weight : bins)
        total += float(weight);
    reading.has_samples = total > 0;
    float bits_float;
    std::memcpy(&bits_float, &peak_bits, sizeof(bits_float));
    reading.peak = std::isfinite(bits_float) ? bits_float : 0.f;
    if (!reading.has_samples) {
        reading.target = std::clamp(reading.requested, tone.adapt_min, std::max(tone.adapt_min, tone.adapt_max));
        return reading;
    }
    // Weighted means over percentile bands of the cumulative weight.
    const auto band_mean = [&](float low, float high) {
        float sum = 0, weight_sum = 0, cumulative = 0;
        for (std::size_t bin = 0; bin < bins.size(); bin++) {
            const float weight = float(bins[bin]);
            const float from = std::max(cumulative, low * total), to = std::min(cumulative + weight, high * total);
            cumulative += weight;
            if (to > from) {
                sum += (to - from) * luminance_of(bin);
                weight_sum += to - from;
            }
        }
        return weight_sum > 0 ? sum / weight_sum : 0.f;
    };
    constexpr float highlight_share = .001f;
    // The band drops only the brightest half percent (a sun, hot specks): a planet filling a
    // fifth of the frame must count in full, or the meter brightens it as if it were sky.
    reading.luminance = band_mean(0.f, .995f) + tone.highlight_bias * band_mean(1.f - highlight_share, 1.f);
    // The strength damps the change in stops, so half strength halves every excursion.
    reading.requested = reading.luminance > 0 ? std::pow(tone.meter_key / reading.luminance, tone.adapt_strength) : 1.f;
    reading.target = std::clamp(reading.requested, tone.adapt_min, std::max(tone.adapt_min, tone.adapt_max));
    reading.limited = reading.requested != reading.target;
    return reading;
}

float settle_target(float current_target, float requested_target, float deadzone_stops) {
    if (current_target <= 0 || requested_target <= 0)
        return requested_target;
    return std::abs(std::log2(requested_target / current_target)) >= deadzone_stops ? requested_target : current_target;
}

} // namespace space::render
