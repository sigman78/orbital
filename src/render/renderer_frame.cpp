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

// History is discarded after a camera jump or a sharp turn.
constexpr double history_max_jump = 10.0;
constexpr double history_min_forward_dot = 0.7;

// Exposure metering: log-average luminance from the meter target, mapped to
// a middle-gray target and smoothed.
constexpr float meter_min_weight = 0.004f, meter_key = 0.18f, adaptation_rate = 0.08f;
constexpr float min_adapted_exposure = 0.75f, max_adapted_exposure = 1.75f;

// Shadow map covers the nearest body: the gas giant when close to it, else Earth.
constexpr double giant_shadow_distance = 100.0;
constexpr float giant_shadow_half_size = 70.f, earth_shadow_half_size = 12.f;
constexpr float sun_disc_radius = 3.0f;                         // Frame.sun.w
constexpr float screen_sun_size = 0.008f;                       // Frame.screen_sun.w
constexpr float screen_sun_max_offset = 1.3f;                   // beyond this the sun is off-screen
constexpr float earth_rotation_offset = -0.95f;                 // aligns the day map with the lighting
constexpr double belt_spin_rate = 0.008, rock_spin_rate = 0.08; // radians per simulation second
constexpr float cluster_bound_scale = 1.30f;                    // the tilt/compression matrix has norm below 1.293
constexpr float rock_radius_scale = 0.025f;
constexpr float billboard_min_pixels = 0.06f, billboard_pixel_radius = 1.2f, lod_switch_pixels = 5.f;

Float4 f4(Vec3f v, float w = 0) {
    return {v.x, v.y, v.z, w};
}

Float4 f4(Vec3d v, float w = 0) {
    return f4(to_float(v), w);
}

float size_tail(unsigned id) {
    return id % 137 == 0 ? 9.f : (id % 23 == 0 ? 4.f : 1.f);
}

// Rock tint: slight per-instance albedo variation from the stable ID.
Float4 rock_tint(unsigned id, float alpha) {
    return {.8f + float(id % 7) * .05f, .84f, .78f, alpha};
}

// The belt is tilted and compressed relative to the giant, and spins with time.
struct BeltTransform {
    float cos_spin, sin_spin;
    Vec3f apply(Vec3f v) const {
        const float bx = v.x * cos_spin - v.z * sin_spin, bz = v.x * sin_spin + v.z * cos_spin;
        return {bx, v.y * .7f - bz * .36f, bz * .933f};
    }
};

// Camera-relative frustum tests against non-unit plane normals.
struct ViewVolume {
    Vec3f right, up, forward;
    float tan_x, tan_y, plane_x_scale, plane_y_scale;
    float pixel_padding; // world units of padding per unit of depth (two pixels)

    bool behind(Vec3f p, float radius) const { return dot(p, forward) < -radius; }
    bool outside(Vec3f p, float radius) const {
        const float z = std::max(dot(p, forward), 0.f);
        const float padding = radius + z * pixel_padding;
        return std::abs(dot(p, right)) > z * tan_x + padding * plane_x_scale ||
               std::abs(dot(p, up)) > z * tan_y + padding * plane_y_scale;
    }
};

// True when the target sphere is entirely hidden behind one of the major bodies.
bool fully_occluded(Vec3f target, float target_radius, std::span<const Vec3f> occluders,
                    std::span<const float> occluder_radii) {
    const float target_distance = length(target);
    if (target_distance <= target_radius)
        return false;
    const Vec3f target_direction = target * (1.f / target_distance);
    const float target_angle = std::asin(std::clamp(target_radius / target_distance, 0.f, 1.f));
    for (std::size_t i = 0; i < occluders.size(); ++i) {
        const float occluder_distance = length(occluders[i]), occluder_radius = occluder_radii[i];
        if (occluder_distance <= occluder_radius ||
            target_distance - target_radius <= occluder_distance + occluder_radius)
            continue;
        const float separation = std::acos(
            std::clamp(dot(target_direction, occluders[i] * (1.f / occluder_distance)), -1.f, 1.f));
        const float occluder_angle = std::asin(std::clamp(occluder_radius / occluder_distance, 0.f, 1.f));
        if (separation + target_angle < occluder_angle)
            return true;
    }
    return false;
}

void write_projection(FrameData& frame, const Camera& camera, float aspect) {
    const Vec3f right = to_float(camera.right()), up = to_float(camera.up()), forward = to_float(camera.forward());
    const float tan_half = float(std::tan(camera.vertical_fov * .5));
    const Mat4 view_projection = perspective_matrix(tan_half, aspect, near_z, far_z) * view_matrix(right, up, forward);
    std::memcpy(frame.view_projection, view_projection.m, sizeof frame.view_projection);
    frame.right_tan = f4(right, tan_half);
    frame.up_aspect = f4(up, aspect);
    frame.forward_exposure = f4(forward, 1);
}

// Orthographic light projection looking from the sun at a body, in double
// like the positions it is built from.
void write_light_projection(FrameData& frame, Vec3d center, Vec3d sun, float half_size) {
    const Vec3d forward = normalized(center - sun), r = normalized(Vec3d{-forward.z, 0, forward.x});
    const Vec3d u = cross(r, forward);
    const Vec3d eye = center - forward * (half_size * 3);
    const double depth_range = half_size * 6;
    float* m = frame.light_projection;
    m[0] = float(r.x / half_size), m[4] = float(r.y / half_size), m[8] = float(r.z / half_size);
    m[12] = float(-dot(r, eye) / half_size);
    m[1] = float(-u.x / half_size), m[5] = float(-u.y / half_size), m[9] = float(-u.z / half_size);
    m[13] = float(dot(u, eye) / half_size);
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
    const auto* values = reinterpret_cast<const float*>(luminance_readback.range.cpu);
    float sum = 0;
    unsigned count = 0;
    for (unsigned i = 0; i < luminance_size * luminance_size; i++)
        if (values[i * 4 + 1] > meter_min_weight) {
            sum += values[i * 4];
            count++;
        }
    const float target =
        count ? std::clamp(meter_key / std::exp(sum / float(count)), min_adapted_exposure, max_adapted_exposure) : 1.f;
    adapted_exposure += (target - adapted_exposure) * adaptation_rate;
    meter_pending = false;
}

FrameData Renderer::Impl::build_frame(const FrameInput& input) {
    const Camera& camera = input.camera;
    FrameData frame{};
    write_projection(frame, camera, float(width) / float(height));
    const Float4 jitter = jitter_sequence[frame_index % jitter_count];
    frame.jitter = {jitter.x, jitter.y, previous_frame.jitter.x, previous_frame.jitter.y};
    for (unsigned column = 0; column < 4; column++) {
        frame.view_projection[column * 4] += 2 * frame.jitter.x / float(width) * frame.view_projection[column * 4 + 3];
        frame.view_projection[column * 4 + 1] += 2 * frame.jitter.y / float(height) *
                                                 frame.view_projection[column * 4 + 3];
    }
    std::memcpy(frame.previous_projection, previous_frame.view_projection, sizeof frame.previous_projection);
    frame.previous_camera_delta = f4(camera.position - previous_camera, history_valid ? 1.f : 0.f);
    frame.previous_forward = previous_frame.forward_exposure;
    const Vec3d previous_forward{previous_frame.forward_exposure.x, previous_frame.forward_exposure.y,
                                 previous_frame.forward_exposure.z};
    if (length(camera.position - previous_camera) > history_max_jump ||
        dot(camera.forward(), previous_forward) < history_min_forward_dot)
        frame.previous_camera_delta.w = 0;
    frame.camera_time = {0, 0, 0, float(input.time)};
    frame.forward_exposure.w = input.exposure * (input.auto_exposure ? adapted_exposure : 1);
    frame.sun = f4(system.star.position - camera.position, sun_disc_radius);
    const bool giant_close = length(input.bodies[giant_index].position - camera.position) < giant_shadow_distance;
    const unsigned shadow_body = giant_close ? giant_index : earth_index;
    write_light_projection(frame, input.bodies[shadow_body].position - camera.position,
                           system.star.position - camera.position,
                           giant_close ? giant_shadow_half_size : earth_shadow_half_size);
    frame.options = {float(width), float(height), input.high_quality ? 1.f : 0.f, input.overlay ? 1.f : 0.f};
    for (unsigned i = 0; i < major_body_count; i++)
        frame.bodies[i] = f4(input.bodies[i].position - camera.position, float(input.bodies[i].radius));

    // Sun position in screen space for the lens flare, hidden when a body covers it.
    const Vec3d sun_direction = normalized(system.star.position - camera.position);
    const double forward = dot(sun_direction, camera.forward());
    const double view_depth = std::max(forward, .001);
    frame.screen_sun = {
        float(dot(sun_direction, camera.right()) / (view_depth * frame.right_tan.w * frame.up_aspect.w)),
        float(-dot(sun_direction, camera.up()) / (view_depth * frame.right_tan.w)), forward > 0 ? 1.f : 0.f,
        screen_sun_size};
    for (const auto& body : input.bodies) {
        const Vec3d v = body.position - camera.position;
        const double along = dot(v, sun_direction);
        if (along > 0 && length(v - sun_direction * along) < body.radius)
            frame.screen_sun.z = 0;
    }
    if (std::abs(frame.screen_sun.x) > screen_sun_max_offset || std::abs(frame.screen_sun.y) > screen_sun_max_offset)
        frame.screen_sun.z = 0;
    return frame;
}

// Everything the per-rock classification needs, computed once per frame.
struct RockCullContext {
    ViewVolume view;
    BeltTransform transform;
    Vec3d giant_relative;
    float pixels_per_unit_depth;
    float rock_spin;
    unsigned limit;
};

// Adds one belt rock to the LOD groups or the billboard list, or drops it.
void Renderer::Impl::classify_rock(unsigned id, const RockCullContext& context) {
    const auto& rock = belt[id];
    const Vec3f p = to_float(context.giant_relative + to_double(context.transform.apply(rock.position)));
    const float radius = rock.scale.x * rock_radius_scale * size_tail(id);
    const float z = dot(p, context.view.forward);
    if (z < -radius)
        return;
    const float pixel_radius = radius * context.pixels_per_unit_depth / std::max(z, .1f);
    if (pixel_radius < billboard_min_pixels || context.view.outside(p, radius))
        return;
    if (pixel_radius < billboard_pixel_radius) {
        // Sub-pixel rocks become fixed-size billboards whose coverage fades in
        // with their true size.
        float coverage = pixel_radius * pixel_radius / (billboard_pixel_radius * billboard_pixel_radius);
        coverage *= std::clamp((pixel_radius - billboard_min_pixels) / billboard_min_pixels, 0.f, 1.f);
        distant.push_back({f4(p, radius * billboard_pixel_radius / pixel_radius),
                           {0, 0, 0, float(SurfaceMode::billboard)},
                           rock_tint(id, coverage)});
        return;
    }
    const bool detailed = radius * context.pixels_per_unit_depth * 2 / std::max(z, .1f) > lod_switch_pixels;
    lod_groups[detailed ? rock.variant : 0].push_back(
        {f4(p, radius),
         {rock.rotation.x, rock.rotation.y + context.rock_spin, rock.rotation.z, float(SurfaceMode::billboard)},
         rock_tint(id, 1)});
}

// Fills the per-frame instance list: the major bodies first, then the belt
// rocks grouped by LOD, then the sub-pixel billboards.
BeltBatches Renderer::Impl::cull_belt(const FrameInput& input, const FrameData& frame) {
    const Camera& camera = input.camera;
    instances.clear();
    distant.clear();
    for (auto& group : lod_groups)
        group.clear();
    for (unsigned i = 0; i < major_body_count; i++)
        instances.push_back(
            {frame.bodies[i],
             {0, float(input.bodies[i].rotation_angle) + (i == earth_index ? earth_rotation_offset : 0.f),
              float(system.bodies[i].axial_tilt), float(i)},
             {1, 1, 1, 1}});

    const float belt_angle = float(input.time * belt_spin_rate);
    const float tan_y = frame.right_tan.w, tan_x = tan_y * frame.up_aspect.w;
    // Plane normals are not unit length; scale sphere support accordingly. Two
    // pixels of padding also cover temporal jitter and expanded tiny billboards.
    const RockCullContext context{.view = {.right = to_float(camera.right()),
                                           .up = to_float(camera.up()),
                                           .forward = to_float(camera.forward()),
                                           .tan_x = tan_x,
                                           .tan_y = tan_y,
                                           .plane_x_scale = std::sqrt(1 + tan_x * tan_x),
                                           .plane_y_scale = std::sqrt(1 + tan_y * tan_y),
                                           .pixel_padding = tan_y * 4 / float(height)},
                                  .transform = {std::cos(belt_angle), std::sin(belt_angle)},
                                  .giant_relative = input.bodies[giant_index].position - camera.position,
                                  .pixels_per_unit_depth = float(height) / (2 * frame.right_tan.w),
                                  .rock_spin = float(input.time * rock_spin_rate),
                                  .limit = input.high_quality ? unsigned(belt.size()) : baseline_belt_count};
    Vec3f occluders[major_body_count];
    float occluder_radii[major_body_count];
    for (unsigned i = 0; i < major_body_count; i++) {
        occluders[i] = to_float(input.bodies[i].position - camera.position);
        occluder_radii[i] = float(input.bodies[i].radius);
    }
    for (const auto& cluster : belt_clusters) {
        const Vec3f position = to_float(context.giant_relative + to_double(context.transform.apply(cluster.center)));
        const float radius = cluster.radius * cluster_bound_scale;
        if (context.view.behind(position, radius) || context.view.outside(position, radius) ||
            fully_occluded(position, radius, occluders, occluder_radii))
            continue;
        for (unsigned id : cluster.indices)
            if (id < context.limit)
                classify_rock(id, context);
    }
    BeltBatches batches;
    for (unsigned lod = 0; lod < geometry::lod_count; lod++) {
        batches.bases[lod] = unsigned(instances.size());
        batches.counts[lod] = unsigned(lod_groups[lod].size());
        instances.insert(instances.end(), lod_groups[lod].begin(), lod_groups[lod].end());
    }
    batches.distant_base = unsigned(instances.size());
    batches.distant_count = unsigned(distant.size());
    instances.insert(instances.end(), distant.begin(), distant.end());
    stats.visible_asteroids = unsigned(instances.size() - major_body_count);
    return batches;
}

void Renderer::Impl::draw_mesh(gpu::CommandBuffer* cmd, Root& root, const GpuMesh& mesh, unsigned base,
                               unsigned instance_count) {
    root.base = base;
    root.vertices = mesh.vertices;
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
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
}

void Renderer::Impl::record_shadow_pass(gpu::CommandBuffer* cmd, Root root, const BeltBatches& batches) {
    root.mode = std::uint32_t(SurfaceMode::shadow);
    gpu::begin_render_pass(cmd, {.depth = {.render_view = shadow_map.view, .load = gpu::LoadOp::clear}});
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, pso.shadow);
    const unsigned triangles_before = stats.triangles;
    for (unsigned i = 0; i < major_body_count; i++)
        draw_mesh(cmd, root, spheres[2], i, 1);
    for (unsigned lod = 0; lod < geometry::lod_count; lod++)
        if (batches.counts[lod])
            draw_mesh(cmd, root, rocks[lod], batches.bases[lod], batches.counts[lod]);
    stats.triangles = triangles_before; // shadow geometry is not counted in the frame statistic
    gpu::end_render_pass(cmd);
    gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
}

void Renderer::Impl::record_scene_pass(gpu::CommandBuffer* cmd, Root root, const FrameInput& input,
                                       const FrameData& frame, const BeltBatches& batches) {
    root.mode = std::uint32_t(SurfaceMode::opaque);
    gpu::ColorAttachment color{.render_view = hdr.view, .load = gpu::LoadOp::clear};
    gpu::begin_render_pass(cmd,
                           {.colors = {&color, 1}, .depth = {.render_view = depth.view, .load = gpu::LoadOp::clear}});
    gpu::bind_pso(cmd, pso.background);
    gpu::draw(cmd, root, 3);
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = true});
    gpu::bind_pso(cmd, pso.opaque);
    for (unsigned i = 0; i < major_body_count; i++) {
        const float distance = float(length(input.bodies[i].position - input.camera.position));
        const float projected = float(input.bodies[i].radius) * float(height) / (distance * frame.right_tan.w);
        draw_mesh(cmd, root, spheres[geometry::select_lod(projected, 2)], i, 1);
    }
    for (unsigned lod = 0; lod < geometry::lod_count; lod++)
        if (batches.counts[lod])
            draw_mesh(cmd, root, rocks[lod], batches.bases[lod], batches.counts[lod]);
    gpu::set_depth_stencil(cmd, {.depth_test = true, .depth_write = false});
    gpu::bind_pso(cmd, pso.cloud);
    if (batches.distant_count) {
        root.base = batches.distant_base;
        root.mode = std::uint32_t(SurfaceMode::billboard);
        gpu::draw(cmd, root, 6, batches.distant_count);
        stats.triangles += batches.distant_count * 2;
    }
    root.mode = std::uint32_t(SurfaceMode::cloud);
    draw_mesh(cmd, root, spheres[geometry::lod_count - 1], earth_index, 1);
    gpu::end_render_pass(cmd);
}

void Renderer::Impl::record_post_passes(gpu::CommandBuffer* cmd, Root root, gpu::RenderView* swapchain_view) {
    const unsigned history_write = frame_index % 2;
    root.mode = std::uint32_t(PostMode::tonemap);
    fullscreen_pass(cmd, history[history_write], pso.temporal, root);
    root.mode = std::uint32_t(PostMode::bloom_a);
    fullscreen_pass(cmd, bloom_a, pso.bloom, root);
    root.mode = std::uint32_t(PostMode::bloom_b);
    fullscreen_pass(cmd, bloom_b, pso.bloom, root);
    root.mode = std::uint32_t(PostMode::tonemap);
    fullscreen_pass(cmd, final_image, pso.post, root);
    if (frame_index % meter_interval == 0) {
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
    ORBITAL_ASSERT(input.bodies.size() >= major_body_count);
    gpu::wait_timeline({s.timeline, s.serial});
    s.read_gpu_timings();
    s.apply_metering();
    const auto extent = gpu::get_drawable_extent(s.device);
    if (!extent.x || !extent.y)
        return false;
    s.resize(extent.x, extent.y);
    const auto swap = gpu::acquire(s.device);
    if (!swap.render_view)
        return false;

    const unsigned history_write = s.frame_index % 2;
    s.bind(Slot::history_a, s.history[history_write]);
    s.bind(Slot::history_b, s.history[1 - history_write]);
    const FrameData frame = s.build_frame(input);
    const BeltBatches batches = s.cull_belt(input, frame);
    s.stats.triangles = 0;

    // Per-frame constants and instances live in the dynamic half of the static heap.
    const auto frame_address = reinterpret_cast<std::uint64_t>(s.data.range.gpu) + dynamic_offset;
    std::memcpy(s.data.range.cpu + dynamic_offset, &frame, sizeof frame);
    std::memcpy(s.data.range.cpu + dynamic_offset + instance_offset, s.instances.data(),
                s.instances.size() * sizeof(Instance));
    Root root{
        .frame = frame_address, .vertices = 0, .instances = frame_address + instance_offset, .base = 0, .mode = 0};

    auto* cmd = gpu::begin_commands(s.device);
    const auto stamp = [&](unsigned index) {
        ORBITAL_ASSERT(index < timestamp_count);
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
    s.record_shadow_pass(cmd, root, batches);
    stamp(1);
    s.record_scene_pass(cmd, root, input, frame, batches);
    stamp(2);
    gpu::barrier(cmd, gpu::Stage::color_output, gpu::Access::color_write, gpu::Stage::color_output,
                 gpu::Access::color_read | gpu::Access::color_write);
    // Atmosphere is composited per body, giant first so Earth's stays on top.
    root.mode = 0;
    root.base = giant_index;
    s.fullscreen_pass(cmd, s.hdr, s.pso.atmosphere, root, true);
    root.base = earth_index;
    s.fullscreen_pass(cmd, s.hdr, s.pso.atmosphere, root, true);
    stamp(3);
    gpu::barrier(cmd, gpu::Stage::depth_stencil_tests, gpu::Access::depth_stencil_write, gpu::Stage::fragment,
                 gpu::Access::shader_read);
    s.record_post_passes(cmd, root, swap.render_view);
    stamp(4);
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
    if (!s.width)
        return false;
    gpu::wait_timeline({s.timeline, s.serial});
    auto readback = gpu::create_gpu_heap(s.device, std::uint64_t(s.width) * s.height * 4, gpu::MemoryType::readback);
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
    std::vector<std::uint8_t> rgb(std::size_t(s.width) * s.height * 3);
    for (std::size_t i = 0; i < std::size_t(s.width) * s.height; i++)
        for (unsigned channel = 0; channel < 3; channel++)
            rgb[i * 3 + channel] = rgba[i * 4 + channel];
    gpu::destroy_gpu_heap(readback);
    return assets::save_png(path, s.width, s.height, 3, rgb.data());
}

} // namespace space::render
