#include "render/renderer_impl.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace space::render {

namespace {

// Orthographic light box, camera relative: right and up span the map, forward is the light direction.
struct LightFrame {
    Vec3f centre{}, forward{}, right{}, up{};
    float half_x = 1, half_y = 1, half_depth = 1;
};

// Temporal anti-aliasing sub-pixel offsets, cycled per frame.
constexpr Float4 jitter_sequence[8] = {{0, -.166667f, 0, 0},      {-.25f, .166667f, 0, 0},  {.25f, -.388889f, 0, 0},
                                       {-.375f, -.055556f, 0, 0}, {.125f, .277778f, 0, 0},  {-.125f, -.277778f, 0, 0},
                                       {.375f, .055556f, 0, 0},   {-.4375f, .388889f, 0, 0}};
constexpr unsigned jitter_count = 8;

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
    {
        // Motion streaks: the mote lattice is fixed in space, so the camera's cell and its offset within it are
        // what the shader needs to place the motes camera-relative.
        const double cell = targets::mote_cell_size_units;
        const Vec3d index{std::floor(camera.position.x / cell), std::floor(camera.position.y / cell),
                          std::floor(camera.position.z / cell)};
        frame.camera_lattice = {float(camera.position.x - index.x * cell), float(camera.position.y - index.y * cell),
                                float(camera.position.z - index.z * cell), float(cell)};
        frame.camera_cell = {float(index.x), float(index.y), float(index.z),
                             input.post.motion_streaks ? input.post.motion_streak_intensity : 0.f};
    }
    frame.forward_exposure.w =
        input.tone.exposure * (input.tone.auto_exposure ? adapted_exposure : 1) * tone_curves::base_gain *
        tone_curves::exposure_trim[std::min(unsigned(input.tone.tone_curve), unsigned(ToneCurve::Count) - 1)];
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
        const double scale = std::max(double(input.belt.lod_scale), .05);
        const double u = std::clamp(
            (outside / scale - belt_lod::fade_start) / (belt_lod::fade_end - belt_lod::fade_start), 0.0, 1.0);
        const float weight = input.belt.disc ? float(u * u * (3 - 2 * u)) : 0.f;
        frame.belt_disc = {float(targets::belt_disc_map_size), weight, float(targets::belt_disc_rock_map_size),
                           input.belt.disc ? 1.f : 0.f};
        stats.belt_lod = weight;
    }
    frame.belt_normal = f4(belt_plane_normal);
    frame.options = {float(extent.width), float(extent.height), input.high_quality ? 1.f : 0.f,
                     input.overlay ? 1.f : 0.f};
    for (unsigned i = 0; i < body_count; i++)
        frame.bodies[i] = f4(input.bodies[i].position - camera.position, float(input.bodies[i].radius));
    frame.scene = {float(body_count), float(giant_index), input.belt.light_map ? 1.f : 0.f,
                   input.belt.extinction ? 1.f : 0.f};
    frame.quality = {input.aa.temporal_aa ? 1.f : 0.f, float(input.tone.tone_curve),
                     input.belt_dust.enabled ? 1.f : 0.f, 0};
    frame.dust = {input.belt_dust.density, input.belt_dust.brightness, input.belt_dust.far, input.belt_dust.saturation};
    frame.dust_tint = {input.belt_dust.tint[0], input.belt_dust.tint[1], input.belt_dust.tint[2], 0};
    frame.earth = {input.earth.ocean_roughness, input.earth.glint_intensity, input.earth.cloud_shadow,
                   input.earth.cloud_shadow_softness};
    frame.earth_more = {input.earth.sea_patchiness, input.earth.cloud_opacity, 0, 0};
    frame.lens = {input.sun.glare_intensity, input.sun.ghost_strength, input.sun.starburst_strength,
                  float(std::clamp(input.sun.starburst_blades, 0, 12))};
    frame.sun_disc = {input.sun.sun_disc_radius, input.sun.sun_limb_darkening, 0, 0};
    frame.post = {input.post.bloom ? input.post.bloom_intensity : 0.f, input.post.bloom_threshold,
                  input.post.bloom_knee, input.post.aberration};
    frame.post_more = {input.post.vignette, input.post.grain, input.post.black_offset, 0};
    frame.giant = {input.gas.time_scale, std::max(input.gas.cycle, .1f), input.gas.turbulence,
                   input.gas.flow ? 1.f : 0.f};
    frame.giant_more = {input.gas.haze, input.gas.terminator, input.gas.relief, input.gas.cap_opacity};
    frame.giant_night = {input.gas.lightning_rate, input.gas.lightning,
                         input.gas.polar && polar_caps ? input.gas.cap_size : 0.f, input.gas.cap_blend};
    frame.stars = {star_count && input.sky.catalogue_stars ? 1.f : 0.f,
                   star_count && input.sky.catalogue_stars ? input.sky.star_brightness : 0.f, input.sky.star_saturation,
                   splat_count && input.sky.milky_way ? input.sky.milky_way_brightness : 0.f};
    frame.galaxy = {float(std::clamp(input.sky.milky_way_splats, 0, int(splat_count))), input.sky.dust_amplitude,
                    input.sky.dust_scale, 0};
    frame.galaxy_more = {input.sky.dust_lacunarity, input.sky.dust_gain, float(galaxy_divisor),
                         input.sky.milky_way_contrast};
    frame.giant_layers = {input.gas.layers ? input.gas.layer_lift : 0.f, input.gas.layer_shadow,
                          input.gas.streaks ? input.gas.streak_strength : 0.f, 0};

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
    p.billboard = {geometry::rock_level_thresholds[4], input.belt.billboard_radius(),
                   belt_culling::billboard::min_pixels, frame.belt_disc.y};
    const unsigned tier_count = (input.high_quality ? high_quality : baseline_quality).belt_count;
    p.rock_limit = std::min(rock_count, belt_count_override ? belt_count_override : tier_count);
    p.body_count = body_count;
    p.light_in_count_pass = input.belt.splat_light_twice ? 1u : 0u;
    for (unsigned group = 0; group < rock_group_count; group++) {
        p.index_counts[group] = rocks[group].index_count;
        p.first_indices[group] = rocks[group].first_index;
        p.vertex_offsets[group] = rocks[group].vertex_offset;
    }
    scratch.instances = instance_address;
    std::memset(scratch.counts, 0, sizeof scratch.counts);
    std::memset(scratch.cursors, 0, sizeof scratch.cursors);
}

} // namespace space::render
