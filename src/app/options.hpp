#pragma once
#include "core/extent.hpp"
#include "render/settings.hpp"
#include "scene/system.hpp"
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace space::app {
inline constexpr std::string_view usage =
    "ORBITAL - NoGraphicsAPI space demo\n"
    "--seed N --frames N --duration seconds --width W --height H --time seconds --bookmark 0..9\n"
    "--capture file.png --benchmark file.csv --tour --high --no-hud --exposure scale --pan axis --pan-stop-frame N "
    "--rocks N --sun-at X Y (turn the camera so the sun projects there, 1 the frame edge; for lens review)\n"
    "--taa 0|1 --spatial 0|1|2 (off, FXAA, SMAA) --dust 0|1 --disc 0|1 --near-tier 0|1 --wireframe 0|1 --terrain-debug "
    "0..5 (uv, normal, elevation, shadow, morph) --tier-activate px (lower switches to the patches further out, "
    "for the coarse levels) --lod-bias X (patch ranges times 2^X; positive is finer) --terrain-detail X "
    "(the tiles' micro-relief, 0 off) --lod-scale X "
    "--vsync 0|1 "
    "--splat 0..3 --tone "
    "0|1|2 --hdr 0|1|2 (SDR, scRGB, HDR10; needs the OS in HDR) "
    "--maximize-at N "
    "--fullscreen-at "
    "N\n"
    "--galaxy 0|1|2 (splats, texture layers, original full resolution) --galaxy-view -180..180 (longitude, sky-only "
    "view)\n"
    "--belt-sun-view (sun through the gas giant's belt, for bloom/occlusion checks)\n"
    "--back units (move away from the bookmark's body along its line, aimed at it) --fov-div X (telescope-like zoom)\n"
    "--shots file (one shot per line: key=value tokens named as the options above, e.g. bookmark=3 frames=80 "
    "capture=mars.png; the command line sets the defaults) --report file.json (per-shot readings) --headless "
    "(hidden window)\n"
    "Controls: RMB mouse look; MMB hold telescope (5x); WASD move; Q/E vertical; Shift fast; 1-8 bookmarks; O orbit; F "
    "free;\n"
    "T tour; Z slow travel; Space pause; +/- exposure; X auto exposure; F1 HUD; F2 quality; F3 belt light map; F4 belt "
    "extinction;\n"
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
    unsigned taa = render::AntiAliasingSettings{}.temporal_aa; // temporal anti-aliasing on
    unsigned dust = render::BeltDustSettings{}.enabled;        // volumetric belt dust
    unsigned disc = render::BeltSettings{}.disc;               // far-belt disc LOD
    unsigned near_tier = render::TerrainSettings{}.near_tier;  // the minor planet's patches close in
    unsigned terrain_debug = 0;                                // the patch path's debug view (TerrainSettings::debug)
    float tier_activate = render::TerrainSettings{}.activate_pixels;        // where the patches take the body over
    float lod_bias = render::TerrainSettings{}.lod_bias;                    // the patch split ranges, times 2^bias
    float terrain_detail = render::TerrainSettings{}.detail;                // the tiles' micro-relief, 0 off
    unsigned wireframe = render::TerrainSettings{}.wireframe;               // their quad grid drawn over the surface
    float lod_scale = render::BeltSettings{}.lod_scale;                     // far-belt fade distance scale
    unsigned spatial = unsigned(render::AntiAliasingSettings{}.spatial_aa); // spatial pass: 0 off, 1 FXAA, 2 SMAA
    std::optional<double> galaxy_view;           // galactic longitude in degrees; absent means normal camera
    std::optional<std::array<double, 2>> sun_at; // frame position the camera turns to put the sun at
    unsigned galaxy = unsigned(render::SkySettings{}.galaxy_mode);
    unsigned splat = unsigned(render::BeltSettings{}.splat_mode); // initial splat cut-off index
    unsigned tone = unsigned(render::ToneSettings{}.tone_curve);  // initial tone curve (PBR Neutral)
    unsigned hdr = unsigned(render::ToneSettings{}.hdr_output);   // swapchain output: 0 SDR, 1 scRGB, 2 HDR10
    unsigned pan_stop_frame = 0;                                  // deterministic movement-to-rest regression
    float pan = 0;            // lateral drift as a fraction of the flight speed, stepped at a fixed 60 Hz for captures
    unsigned maximize_at = 0; // > 0 maximizes the window after this many frames, to test resizing in captures
    unsigned fullscreen_at = 0; // > 0 enters borderless fullscreen after this many frames
    double back = 0;            // move the camera this far from the bookmark's body along its line, aimed at it
    double fov_div = 1;         // divide the field of view, as the telescope does, for zoomed checks
    bool tour = false, high = false, no_hud = false, help = false;
    bool ui = false;            // start with the control panel shown
    bool belt_sun_view = false; // repeatable view through the gas giant's belt toward the sun
    bool headless = false;      // the window is created hidden: scripted runs without a window on screen
    std::filesystem::path capture, benchmark;
    std::filesystem::path shots;  // a shot list to run in one process, one shot per line
    std::filesystem::path report; // JSON with each shot's readings, written when the run ends
};

// argv includes the program name. Invalid options log an option-specific error.
std::optional<Options> parse_options(int argc, const char* const* argv);

// One line of a shot list applied over the base options: key=value tokens named
// as the command line options without their dashes (bookmark=3 frames=80
// capture=mars.png time=0), a bare key for a flag (high), commas for a
// multi-value option (sun-at=0.7,-0.7), and a name=... token for the report.
// Blank lines and lines starting with # are skipped (nullopt with an empty name).
struct Shot {
    std::string name;
    Options options;
};
std::optional<Shot> parse_shot(const Options& base, std::string_view line, unsigned line_number);
// The whole list; nullopt after logging the first line that does not parse.
std::optional<std::vector<Shot>> load_shots(const Options& base, const std::filesystem::path& path);
} // namespace space::app
