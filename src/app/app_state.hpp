#pragma once
#include "app/camera.hpp"
#include "core/math.hpp"

#include <array>
#include <filesystem>
#include <string_view>

// Interactive state that key presses, the control panel and the frame loop
// share, with the constants the controls are defined in terms of.
namespace space::app {

namespace exposure_keys {
inline constexpr Range<float> range{0.05f, 8.0f};
inline constexpr float step = 1.1f; // per +/- key press
} // namespace exposure_keys

namespace control {
inline constexpr double max_frame_seconds = 0.1; // a stall must not advance the simulation by more than this
inline constexpr float fast_speed_scale = 4.0f;  // Shift
inline constexpr double orbit_zoom_radii = 2.3;  // O key orbits at this many body radii
} // namespace control

inline constexpr std::string_view hotkey_capture_path = "captures/orbital.png";

// Projected rock radius, in pixels, below which rocks draw as disc splats; 0 means never (F6 cycles).
inline constexpr std::array<float, 4> splat_radii{0.f, 1.2f, 2.5f, 4.f};

struct AppState {
    Camera camera;
    BodyStates bodies;
    bool running = true, paused = false, high = false, overlay = true, auto_exposure = true;
    bool belt_light_map = true, belt_extinction = true;        // development toggles for the belt shading
    bool belt_dust = true;                                     // volumetric belt dust (F11)
    float dust_density = 1, dust_brightness = 1, dust_far = 1; // panel multipliers on the dust
    float dust_saturation = 1;
    float dust_tint[3] = {1, 1, 1};
    bool belt_disc = true;          // far-belt disc LOD (--disc)
    float belt_lod_scale = 1;       // distance scale of the fade to the baked far belt
    bool temporal_aa = true;        // F5
    unsigned spatial_aa = 2;        // 0 off, 1 FXAA, 2 SMAA (F9)
    unsigned splat_mode = 2;        // index into splat_radii
    bool splat_light_twice = false; // light splats in the count pass too
    unsigned tone_curve = 2;        // 0 ACES filmic, 1 AgX, 2 PBR Neutral
    bool show_ui = false;           // control panel (F12, --ui)
    bool vsync = true;              // presentation waits for the display (--vsync); off for measurements
    float ocean_roughness = .18f, glint_intensity = 1.f, sea_patchiness = .5f;    // Earth panel: sea state
    float cloud_shadow = .6f, cloud_shadow_softness = 1.5f, cloud_opacity = .88f; // Earth panel: clouds
    float glare_intensity = 1.f, ghost_strength = 1.f, starburst_strength = 1.f;  // Sun & lens panel
    int starburst_blades = 6;
    float sun_disc_radius = .007f, sun_limb_darkening = .6f;
    bool bloom = true; // Post FX panel
    float bloom_intensity = .24f, bloom_threshold = .85f, bloom_knee = .5f;
    float aberration = 1.f, vignette = .17f, grain = .010f, black_offset = .3f;
    bool motion_streaks = true;
    float motion_streak_intensity = 1.f;
    bool catalogue_stars = true; // Sky panel
    float star_brightness = 2.f, star_saturation = .5f;
    bool milky_way = true;
    float milky_way_brightness = .1f, milky_way_contrast = 1.f;
    float dust_amplitude = 2.f, dust_scale = 1.4f, dust_lacunarity = 3.f, dust_gain = .7f;
    int milky_way_splats = 4096, galaxy_divisor = 4;
    bool gas_flow = true; // Gas giant panel
    float gas_time_scale = 1500.f, gas_cycle = 12.f, gas_turbulence = 1.f;
    float gas_haze = .08f, gas_terminator = .08f, gas_relief = 6.f;
    float gas_lightning_rate = 4.f, gas_lightning = 1.f;
    bool gas_polar = true;
    float gas_cap_size = 1.5f, gas_cap_blend = .5f, gas_cap_opacity = 1.f;
    bool gas_layers = true;
    float gas_layer_lift = .0015f, gas_layer_shadow = .3f;
    bool gas_streaks = true;
    float gas_streak_strength = 1.f;
    float pan = 0; // lateral drift added to the move axis (--pan)
    float exposure = 1.0f;
    unsigned selected_body = 0;
    std::filesystem::path capture_request;
};

} // namespace space::app
