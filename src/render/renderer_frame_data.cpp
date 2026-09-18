#include "render/frame_calculations.hpp"
#include "render/renderer_impl.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace space::render {

namespace {

// Exposure gain ahead of the tone curves, read by build_frame. The base gain
// brings the Hill ACES fit, which sits about a stop under the Narkowicz
// approximation it replaced, back toward the earlier brightness. The per-curve
// trims were fitted on captures so the shadows of each curve match ACES (see
// docs/DECISIONS.md); switching curves then compares rendering intent rather
// than brightness.
namespace tone_curves {
inline constexpr float base_gain = 1.5f;
} // namespace tone_curves

// Sun disc and lens flare, read by build_frame.
namespace sun_flare {
inline constexpr float disc_radius = 3.0f;   // Frame.sun.w
inline constexpr float screen_size = 0.008f; // Frame.screen_sun.w
} // namespace sun_flare

// Belt motion and per-rock culling limits, read by write_cull_scratch.
// Body draw tiers, read by cull_bodies. The detail weight runs over the smallest
// mesh level's range (the level thresholds are 24, 80 and 240 pixels of radius).
// The cloud shell is drawn while it is large enough for its parallax to show
// (below the limit the ground pass folds the clouds in, less than a pixel off).
namespace body_tiers {
inline constexpr float detail_from_pixels = 24.f, detail_to_pixels = 80.f;
inline constexpr float shell_min_pixels = 80.f;
inline constexpr float grow_hysteresis = 1.15f, shrink_hysteresis = .85f;
} // namespace body_tiers

namespace belt_culling {
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

} // namespace

FrameData Renderer::Impl::build_frame(const FrameInput& input) {
    const CameraView& camera = input.camera;
    FrameData frame{};
    const CameraHistory previous{.position = previous_camera,
                                 .forward = {previous_frame.forward_exposure.x, previous_frame.forward_exposure.y,
                                             previous_frame.forward_exposure.z},
                                 .vertical_fov = previous_vertical_fov,
                                 .cut_serial = previous_camera_cut,
                                 .valid = history_valid,
                                 .temporal_aa = previous_frame.quality.x > .5f};
    if (camera.cut_serial != previous_camera_cut)
        frames_since_cut = 0;
    const auto view = prepare_camera(camera, previous, extent, frames_since_cut, input.aa.temporal_aa,
                                     targets::depth.min, targets::depth.max, targets::mote_cell_size_units);
    std::memcpy(frame.view_projection, view.projection.m, sizeof frame.view_projection);
    frame.right_tan = f4(view.right, view.tan_half_fov);
    frame.up_aspect = f4(view.up, view.aspect);
    frame.forward_exposure = f4(view.forward, 1);
    frame.jitter = {view.jitter_x, view.jitter_y, previous_frame.jitter.x, previous_frame.jitter.y};
    std::memcpy(frame.previous_projection, previous_frame.view_projection, sizeof frame.previous_projection);
    frame.previous_camera_delta = f4(view.delta, view.history_valid ? 1.f : 0.f);
    frame.previous_forward = previous_frame.forward_exposure;
    frame.camera_time = {0, 0, 0, float(input.time)};
    frame.camera_lattice = f4(view.lattice_offset, float(targets::mote_cell_size_units));
    frame.camera_cell = f4(view.lattice_cell, input.post.motion_streaks && view.history_valid && view.moving
                                                  ? input.post.motion_streak_intensity
                                                  : 0.f);
    frame.forward_exposure.w =
        input.tone.exposure * (input.tone.auto_exposure ? adapted_exposure : 1) * tone_curves::base_gain *
        input.tone.curve_trim[std::min(unsigned(input.tone.tone_curve), unsigned(ToneCurve::Count) - 1)];
    stats.exposure.applied = frame.forward_exposure.w;
    stats.exposure.automatic = input.tone.auto_exposure;
    frame.sun = f4(system.star.position - camera.position, sun_flare::disc_radius);
    const Vec3d sun_relative = system.star.position - camera.position;
    const auto body_light = prepare_body_light(input.bodies[showcase.earth()].position - camera.position,
                                               input.bodies[showcase.giant()].position - camera.position,
                                               input.bodies[showcase.desert()].position - camera.position,
                                               sun_relative);
    const auto light_matrix = light_projection(body_light);
    std::memcpy(frame.light_projection, light_matrix.m, sizeof frame.light_projection);
    const auto& ring = system.belts.front();
    auto belt = prepare_belt(ring, input.bodies[showcase.belt_parent()].position - camera.position, sun_relative,
                             input.belt.disc, input.belt.lod_scale);
    // Frozen culling keeps the cull camera, and with it the far-tier weight, of
    // the frame it was switched on in, so the disc and the splats it replaces
    // stay in step while the live camera inspects them.
    if (!input.belt.freeze_culling)
        frozen_cull.reset();
    else if (!frozen_cull)
        frozen_cull = FrozenCull{.camera = camera, .tan_y = view.tan_half_fov, .disc_weight = belt.disc_weight};
    if (frozen_cull)
        belt.disc_weight = frozen_cull->disc_weight;
    const auto belt_light_matrix = light_projection(belt.light);
    const auto belt_disc_matrix = light_projection(belt.disc);
    std::memcpy(frame.belt_light_projection, belt_light_matrix.m, sizeof frame.belt_light_projection);
    std::memcpy(frame.belt_disc_projection, belt_disc_matrix.m, sizeof frame.belt_disc_projection);
    frame.belt_light = {belt.light.half_x, belt.light.half_y, float(targets::belt_light_map_size),
                        belt.light_slice_depth};
    frame.belt_ring = {float(ring.inner_radius), float(ring.outer_radius), float(ring.density), float(ring.thickness)};
    frame.belt_disc_right = f4(belt.disc.right, belt.disc.half_x);
    frame.belt_disc_up = f4(belt.disc.up, belt.disc.half_y);
    frame.belt_disc = {float(targets::belt_disc_map_size), belt.disc_weight, float(targets::belt_disc_rock_map_size),
                       input.belt.disc ? 1.f : 0.f};
    stats.frame.belt_lod = belt.disc_weight;
    frame.belt_normal = f4(belt_plane_normal);
    frame.options = {float(extent.width), float(extent.height), input.high_quality ? 1.f : 0.f,
                     input.overlay ? 1.f : 0.f};
    for (unsigned i = 0; i < body_count; i++)
        frame.bodies[i] = f4(input.bodies[i].position - camera.position, float(input.bodies[i].radius));
    frame.scene = {float(body_count), float(showcase.giant()), input.belt.light_map ? 1.f : 0.f,
                   input.belt.extinction ? 1.f : 0.f};
    frame.quality = {input.aa.temporal_aa ? 1.f : 0.f, float(input.tone.tone_curve),
                     input.belt_dust.enabled ? 1.f : 0.f, input.sun.ambient_fill};
    frame.dust = {input.belt_dust.density, input.belt_dust.brightness, input.belt_dust.far, input.belt_dust.saturation};
    frame.dust_tint = {input.belt_dust.tint[0], input.belt_dust.tint[1], input.belt_dust.tint[2], 0};
    // The jitter seed counts frames, not seconds: a paused simulation still decorrelates the
    // marches for the temporal pass, and a shot's frame gets the same seed wherever it sits.
    frame.sequence = {float(frames_since_cut % 4096), float(input.terrain.debug), 0, 0};
    frame.earth = {input.earth.ocean_roughness, input.earth.glint_intensity, input.earth.cloud_shadow,
                   input.earth.cloud_shadow_softness};
    frame.earth_more = {input.earth.sea_patchiness, input.earth.cloud_opacity, 0, 0};
    const float flare = input.sun.lens_flare ? 1.f : 0.f; // off zeroes every stack element; the glare stays
    frame.lens = {input.sun.glare_intensity, input.sun.ghost_strength * flare, input.sun.starburst_strength * flare,
                  float(std::clamp(input.sun.starburst_blades, 0, 12))};
    frame.sun_disc = {input.sun.sun_disc_radius, input.sun.sun_limb_darkening, input.sun.ring_strength * flare,
                      input.sun.crescent_strength * flare};
    frame.lens_more = {input.sun.mini_crescent_strength * flare, input.sun.streak_strength * flare,
                       input.sun.ghost_spread, input.sun.ghost_size};
    frame.post = {input.post.bloom ? input.post.bloom_intensity : 0.f, input.post.bloom_threshold,
                  input.post.bloom_knee, input.post.aberration};
    frame.post_more = {input.post.vignette, input.post.grain, input.post.black_offset, input.sun.flare_adaptation};
    frame.giant = {input.gas.time_scale, std::max(input.gas.cycle, .1f), input.gas.turbulence,
                   input.gas.flow ? 1.f : 0.f};
    frame.giant_more = {input.gas.haze, input.gas.terminator, input.gas.relief, input.gas.cap_opacity};
    frame.giant_night = {input.gas.lightning_rate, input.gas.lightning,
                         input.gas.polar && polar_caps ? input.gas.cap_size : 0.f, input.gas.cap_blend};
    const bool original_galaxy = input.sky.galaxy_mode == GalaxyMode::OriginalTexture && galaxy_original_available;
    const bool texture_galaxy = original_galaxy ||
                                (input.sky.galaxy_mode == GalaxyMode::TextureLayers && galaxy_layers_available);
    frame.galaxy_layers = {input.sky.galaxy_low, input.sky.galaxy_clouds, input.sky.galaxy_filaments, 0};
    frame.stars = {star_count && input.sky.catalogue_stars ? 1.f : 0.f,
                   star_count && input.sky.catalogue_stars ? input.sky.star_brightness : 0.f, input.sky.star_saturation,
                   (texture_galaxy || splat_count) && input.sky.milky_way ? input.sky.milky_way_brightness : 0.f};
    frame.galaxy = {float(std::clamp(input.sky.milky_way_splats, 0, int(splat_count))), input.sky.dust_amplitude,
                    input.sky.dust_scale,
                    original_galaxy  ? 2.f
                    : texture_galaxy ? 1.f
                                     : 0.f};
    frame.galaxy_more = {input.sky.dust_lacunarity, input.sky.dust_gain, float(galaxy_divisor),
                         input.sky.milky_way_contrast};
    frame.giant_layers = {input.gas.layers ? input.gas.layer_lift : 0.f, input.gas.layer_shadow,
                          input.gas.streaks ? input.gas.streak_strength : 0.f, 0};

    const auto sun = project_sun(camera, system.star.position, input.bodies, view.tan_half_fov, view.aspect);
    frame.screen_sun = {sun.x, sun.y, sun.visible ? 1.f : 0.f, sun_flare::screen_size};
    // The output encoding for the composite and the present pass; the overlay packs the same into its root.
    const float paper_white = std::max(input.tone.paper_white_nits, 1.f);
    // The auto exposure's multiplier alone, so the composite can hold the flare out of it.
    frame.display = {float(hdr_output), paper_white, std::max(input.tone.peak_nits / paper_white, 1.f),
                     input.tone.auto_exposure ? adapted_exposure : 1.f};
    ui_display = unsigned(hdr_output) | (unsigned(std::lround(paper_white)) << 8);
    // The glass is lit by the sun wherever it is unless a body covers it; it blurs regardless.
    const bool glass = input.post.dirty_glass && lens_dirt;
    frame.lens_stack = {input.sun.lens_flare ? float(flare_divisor) : 0.f, input.sun.flare_saturation,
                        glass && sun.lit ? input.post.dirt_light : 0.f, glass ? input.post.dirt_blur : 0.f};
    return frame;
}

// The body instances occupy the first slots of the per-frame instance list.
void Renderer::Impl::write_body_instances(const FrameInput& input, const FrameData& frame) {
    instances.clear();
    for (unsigned i = 0; i < body_count; i++) {
        const BodyClass body_class = system.bodies[i].body_class;
        // Moonlets are dark, reddish rock; the tint also seeds their texture offset.
        const Float4 tint = body_class == BodyClass::Moonlet ? Float4{.34f, .30f, .27f, 1} : Float4{1, 1, 1, 1};
        // The step back to the previous frame: the orbit's in world space, the spin's about the axis.
        const BodyState& previous = previous_bodies_valid ? previous_bodies[i] : input.bodies[i];
        instances.push_back({frame.bodies[i],
                             {0, float(input.bodies[i].rotation_angle), float(system.bodies[i].axial_tilt),
                              float(surface_kind(body_class))},
                             tint,
                             f4(previous.position - input.bodies[i].position),
                             {0, float(previous.rotation_angle - input.bodies[i].rotation_angle), 0, 0}});
    }
}

// Which bodies can be on screen and at what mesh level. The test runs at the
// cull camera, so a frozen cull holds the bodies as it holds the rocks, and it
// covers the atmosphere shell (up to 1.8% over the radius, the minor planet's
// high haze 6%) so a shell whose body is just off screen still draws.
void Renderer::Impl::cull_bodies(const FrameInput& input, const FrameData& frame) {
    const CameraView& camera = frozen_cull ? frozen_cull->camera : input.camera;
    const float tan_y = frozen_cull ? frozen_cull->tan_y : frame.right_tan.w, tan_x = tan_y * frame.up_aspect.w;
    const geometry::Frustum frustum = view_frustum(camera, tan_x, tan_y);
    body_visible.reset();
    stats.frame.bodies_drawn = 0;
    for (unsigned i = 0; i < body_count; i++) {
        const Vec3d relative = input.bodies[i].position - camera.position;
        const float radius = float(input.bodies[i].radius);
        const float shell = system.bodies[i].body_class == BodyClass::MinorPlanet ? 1.07f : 1.02f;
        body_visible[i] = geometry::sphere_in_frustum(frustum, to_float(relative), radius * shell);
        const float distance = float(length(relative));
        const float projected = radius * float(extent.height) / (distance * tan_y);
        body_level[i] = geometry::select_lod(projected, body_level[i]);
        const float detail = std::clamp((projected - body_tiers::detail_from_pixels) /
                                            (body_tiers::detail_to_pixels - body_tiers::detail_from_pixels),
                                        0.f, 1.f);
        body_detail[i] = detail * detail * (3 - 2 * detail);
        body_shell[i] = projected >= body_tiers::shell_min_pixels *
                                         (body_shell[i] ? body_tiers::shrink_hysteresis : body_tiers::grow_hysteresis);
        stats.frame.bodies_drawn += body_visible[i] ? 1 : 0;
    }
}

// Everything the GPU culling pass needs this frame, mirroring the CPU
// frustum test it replaced: plane normals are not unit length, so sphere
// support is scaled, and two pixels of padding cover temporal jitter and
// expanded tiny billboards.
// The rocks' states for this frame, stepped by the sweep into this frame's
// staging slot, which the frame copies into the device slice. Runs before the
// wait for the previous frame, so the write overlaps that frame's GPU work; the
// previous frame reads the other slot. A standing state is written only into a
// slot that does not hold it yet.
void Renderer::Impl::write_belt_state(const FrameInput& input) {
    const unsigned slot = frame_index & 1, count = active_rock_count(input);
    auto* out = reinterpret_cast<RockState*>(buffers.belt_state_staging.range().cpu +
                                             slot * std::uint64_t(belt_capacity) * belt_state_stride);
    if (!belt_motion.advance(input.time, count, out) && belt_staging_version[slot] != belt_motion.version())
        belt_motion.write(count, out);
    belt_staging_version[slot] = belt_motion.version();
}

unsigned Renderer::Impl::active_rock_count(const FrameInput& input) const {
    const unsigned tier_count = (input.high_quality ? high_quality : baseline_quality).belt_count;
    return std::min(rock_count, belt_count_override ? belt_count_override : tier_count);
}

void Renderer::Impl::write_cull_scratch(const FrameInput& input, const FrameData& frame, CullScratch& scratch,
                                        std::uint64_t instance_address) {
    // build_frame has already captured or released the frozen cull camera; the
    // frame's far-tier weight is the frozen one when it holds.
    const CameraView& camera = frozen_cull ? frozen_cull->camera : input.camera;
    const float tan_y = frozen_cull ? frozen_cull->tan_y : frame.right_tan.w, tan_x = tan_y * frame.up_aspect.w;
    CullParams& p = scratch.params;
    p = {};
    p.right = f4(to_float(camera.right), tan_x);
    p.up = f4(to_float(camera.up), tan_y);
    p.forward = f4(to_float(camera.forward), tan_y * 4 / float(extent.height));
    p.view = {float(extent.height) / (2 * tan_y), std::sqrt(1 + tan_x * tan_x), std::sqrt(1 + tan_y * tan_y), 0};
    // Rocks stay live camera relative for the draw; the offset moves the tests to the cull camera.
    p.giant = f4(input.bodies[showcase.belt_parent()].position - input.camera.position);
    p.freeze = f4(input.camera.position - camera.position);
    const unsigned parent = showcase.belt_parent();
    p.giant_motion = previous_bodies_valid ? f4(previous_bodies[parent].position - input.bodies[parent].position)
                                           : Float4{0, 0, 0, 0};
    p.levels = {geometry::rock_level_thresholds[0], geometry::rock_level_thresholds[1],
                geometry::rock_level_thresholds[2], geometry::rock_level_thresholds[3]};
    p.billboard = {geometry::rock_level_thresholds[4], input.belt.billboard_radius(),
                   belt_culling::billboard::min_pixels, frame.belt_disc.y};
    p.rock_limit = active_rock_count(input);
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
