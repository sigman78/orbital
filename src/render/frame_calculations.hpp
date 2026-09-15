#pragma once
#include "core/extent.hpp"
#include "render/camera_view.hpp"
#include "render/settings.hpp"
#include "scene/geometry.hpp"
#include "scene_shared.h" // the exposure meter's histogram layout
#include <cstdint>
#include <span>

namespace space {
struct BeltDescription;
struct BodyState;
} // namespace space

namespace space::render {

struct CameraHistory {
    Vec3d position{}, forward{};
    double vertical_fov = 0;
    std::size_t cut_serial = 0;
    bool valid = false, temporal_aa = false;
};

struct PreparedCamera {
    Mat4 projection;
    Vec3f right{}, up{}, forward{};
    float tan_half_fov = 0, aspect = 1;
    float jitter_x = 0, jitter_y = 0;
    Vec3d delta{};
    Vec3f lattice_offset{}, lattice_cell{};
    bool history_valid = false, moving = false;
};

PreparedCamera prepare_camera(const CameraView& camera, const CameraHistory& previous, Extent2D extent,
                              unsigned frame_index, bool temporal_aa, float near_plane, float far_plane,
                              unsigned lattice_cell_size);

// Orthographic box, camera relative. No shader layout or resource handles.
struct LightFrame {
    Vec3f centre{}, forward{}, right{}, up{};
    float half_x = 1, half_y = 1, half_depth = 1;
};

// The view frustum in camera-relative space: the near plane and the four sides,
// as geometry::sphere_in_frustum takes them. tan_x and tan_y are the half-angle
// tangents; the far plane is left out, since nothing drawn reaches it.
geometry::Frustum view_frustum(const CameraView& camera, float tan_x, float tan_y);

LightFrame body_light_frame(Vec3d centre, Vec3d sun, float half_size);
LightFrame prepare_body_light(Vec3d earth, Vec3d giant, Vec3d desert, Vec3d sun);
Mat4 light_projection(const LightFrame& light);

inline constexpr Vec3f belt_plane_normal{0.f, .933f, .36f};
struct PreparedBelt {
    LightFrame light, disc;
    float light_slice_depth = 0, disc_weight = 0;
};
PreparedBelt prepare_belt(const BeltDescription& ring, Vec3d centre, Vec3d sun, bool disc_enabled, float lod_scale);

struct ScreenSun {
    float x = 0, y = 0;
    bool visible = false; // in front of the camera, within reach of the frame and not covered
    bool lit = true;      // no body covers the sun, wherever it is
};
ScreenSun project_sun(const CameraView& camera, Vec3d sun, std::span<const BodyState> bodies, float tan_half_fov,
                      float aspect);

// The exposure meter's reading from its histogram: ORBITAL_METER_BINS bins of
// fixed-point centre weight over log2 luminance, and the brightest tap as float bits.
// The metered luminance is the weighted mean below the 99.5th percentile, which no
// single outlier can move, plus the highlight bias share of the mean of
// the brightest tenth of a percent, which is where a sun in frame shows; the
// request maps the key to 1x, damped in stops by the strength and held within the
// adaptation range.
struct MeterReading {
    float luminance = 0, peak = 0;   // metered luminance and the brightest tap anywhere
    float requested = 1, target = 1; // before and after the range clamp
    bool has_samples = false, limited = false;
};
MeterReading meter_exposure(std::span<const std::uint32_t> bins, std::uint32_t peak_bits, const ToneSettings& tone);
// The dead zone: the target the adaptation follows moves to the meter's request only
// once the request differs from it by at least `deadzone_stops`, so a composition
// drifting by a fraction of a stop leaves the exposure alone.
float settle_target(float current_target, float requested_target, float deadzone_stops);

} // namespace space::render
