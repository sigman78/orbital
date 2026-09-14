#pragma once
#include "core/extent.hpp"
#include "render/camera_view.hpp"
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
    bool visible = false;
};
ScreenSun project_sun(const CameraView& camera, Vec3d sun, std::span<const BodyState> bodies, float tan_half_fov,
                      float aspect);

} // namespace space::render
