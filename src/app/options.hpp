#pragma once
#include "core/extent.hpp"
#include "render/settings.hpp"
#include "scene/system.hpp"
#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace space::app {
inline constexpr std::string_view usage =
    "ORBITAL - NoGraphicsAPI space demo\n"
    "--seed N --frames N --duration seconds --width W --height H --time seconds --bookmark 0..5\n"
    "--capture file.png --benchmark file.csv --tour --high --no-hud --exposure scale --pan axis --pan-stop-frame N "
    "--rocks N --sun-at X Y (turn the camera so the sun projects there, 1 the frame edge; for lens review)\n"
    "--taa 0|1 --spatial 0|1|2 (off, FXAA, SMAA) --dust 0|1 --disc 0|1 --lod-scale X --vsync 0|1 --splat 0..3 --tone "
    "0|1|2 "
    "--maximize-at N "
    "--fullscreen-at "
    "N\n"
    "--galaxy 0|1|2 (splats, texture layers, original full resolution) --galaxy-view -180..180 (longitude, sky-only "
    "view)\n"
    "--belt-sun-view (sun through the gas giant's belt, for bloom/occlusion checks)\n"
    "Controls: RMB mouse look; WASD move; Q/E vertical; Shift fast; 1-6 bookmarks; O orbit; F free;\n"
    "T tour; Space pause; +/- exposure; X auto exposure; F1 HUD; F2 quality; F3 belt light map; F4 belt extinction;\n"
    "F5 temporal AA; F6 rock splat cut-off; F7 splat lighting in both cull passes; F8 tone curve;\n"
    "F9 spatial AA (off, FXAA, SMAA); Alt+Enter borderless fullscreen;\n"
    "F10 capture; F11 belt dust; F12 control panel (--ui shows it at start); Esc exit.";

struct Options {
    std::uint64_t seed = showcase_seed;
    unsigned frame_limit = 0;
    Extent2D size = Extent2D{1600, 900};
    double fixed_time = -1; // >= 0 freezes simulation and exposure adaptation at this time
    double duration = 0;    // > 0 exits after this many wall-clock seconds
    int bookmark = -1;
    float exposure = render::ToneSettings{}.exposure;
    unsigned rocks = 0; // belt override for benchmarks; 0 keeps the quality tiers
    int vsync = -1;     // -1 default: on, except off for benchmarks
    unsigned taa = render::AntiAliasingSettings{}.temporal_aa;              // temporal anti-aliasing on
    unsigned dust = render::BeltDustSettings{}.enabled;                     // volumetric belt dust
    unsigned disc = render::BeltSettings{}.disc;                            // far-belt disc LOD
    float lod_scale = render::BeltSettings{}.lod_scale;                     // far-belt fade distance scale
    unsigned spatial = unsigned(render::AntiAliasingSettings{}.spatial_aa); // spatial pass: 0 off, 1 FXAA, 2 SMAA
    std::optional<double> galaxy_view;           // galactic longitude in degrees; absent means normal camera
    std::optional<std::array<double, 2>> sun_at; // frame position the camera turns to put the sun at
    unsigned galaxy = unsigned(render::SkySettings{}.galaxy_mode);
    unsigned splat = unsigned(render::BeltSettings{}.splat_mode); // initial splat cut-off index
    unsigned tone = unsigned(render::ToneSettings{}.tone_curve);  // initial tone curve (PBR Neutral)
    unsigned pan_stop_frame = 0;                                  // deterministic movement-to-rest regression
    float pan = 0;            // lateral drift as a fraction of the flight speed, stepped at a fixed 60 Hz for captures
    unsigned maximize_at = 0; // > 0 maximizes the window after this many frames, to test resizing in captures
    unsigned fullscreen_at = 0; // > 0 enters borderless fullscreen after this many frames
    bool tour = false, high = false, no_hud = false, help = false;
    bool ui = false;            // start with the control panel shown
    bool belt_sun_view = false; // repeatable view through the gas giant's belt toward the sun
    std::filesystem::path capture, benchmark;
};

// argv includes the program name. Invalid options log an option-specific error.
std::optional<Options> parse_options(int argc, const char* const* argv);
} // namespace space::app
