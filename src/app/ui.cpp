#include "app/ui.hpp"

#include "platform/overlay.hpp"
#include "platform/window.hpp"

#include <algorithm>
#include <vector>

#include "imgui.h"

namespace space::app {

namespace {

constexpr float panel_width = 360;
constexpr const char* bookmark_names[] = {"Earth", "Jupiter", "Moon", "Mars", "Dawn", "Belt"};
static_assert(std::size(bookmark_names) == bookmark_count);

// A collapsing section with its own ID scope: headers push none, so labels
// could otherwise collide across sections (the "Belt" header and button did).
bool section(const char* title) {
    const bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);
    if (open)
        ImGui::PushID(title);
    return open;
}

template <class Enum> void combo(const char* label, Enum& value, std::span<const char* const> items) {
    int index = int(std::min<unsigned>(unsigned(value), unsigned(items.size() - 1)));
    if (ImGui::Combo(label, &index, items.data(), int(items.size())))
        value = Enum(index);
}

} // namespace

Ui::Ui(platform::Window& window) : window_(window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no layout file next to the executable
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendRendererName = "orbital NoGraphicsAPI";
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.Colors[ImGuiCol_WindowBg].w = .88f;
    platform::attach_overlay(window);
}

Ui::~Ui() {
    platform::detach_overlay(window_);
    ImGui::DestroyContext();
}

Ui::FontAtlas Ui::font_atlas() const {
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    return {.rgba = pixels, .width = unsigned(width), .height = unsigned(height)};
}

void Ui::begin_frame() {
    platform::overlay_new_frame();
    ImGui::NewFrame();
}

const ImDrawData* Ui::end_frame() {
    ImGui::Render();
    return ImGui::GetDrawData();
}

bool Ui::wants_keyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool Ui::wants_mouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

void draw_panel(AppState& app, const render::Stats& stats, std::span<const float> recent_frame_ms) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x - panel_width, 0});
    ImGui::SetNextWindowSize({panel_width, io.DisplaySize.y});
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin("ORBITAL", nullptr, flags)) {
        ImGui::End();
        return;
    }
    ImGui::PushItemWidth(150); // leaves room for the labels beside combos and sliders
    if (section("Frame")) {
        std::vector<float> sorted(recent_frame_ms.begin(), recent_frame_ms.end());
        std::sort(sorted.begin(), sorted.end());
        const float p95 = sorted.empty()
                              ? 0
                              : sorted[std::min(sorted.size() - 1, std::size_t(double(sorted.size()) * .95))];
        const float peak = sorted.empty() ? 1 : sorted.back();
        ImGui::Text("%d FPS   %.1f ms (p95 %.1f)", int(1000 / std::max(stats.frame_ms, .1f)), stats.frame_ms, p95);
        ImGui::PlotLines("##frame", recent_frame_ms.data(), int(recent_frame_ms.size()), 0, nullptr, 0, peak * 1.1f,
                         {-1, 60});
        ImGui::Text("GPU %.2f ms", stats.gpu_ms);
        ImGui::Indent();
        ImGui::Text("cull + shadow %.2f   surface %.2f", stats.shadow_ms, stats.surface_ms);
        ImGui::Text("atmosphere %.2f   post %.2f", stats.atmosphere_ms, stats.post_ms);
        ImGui::Unindent();
        ImGui::Text("CPU prepare %.2f ms", stats.prepare_ms);
        ImGui::Checkbox("VSync", &app.vsync);
        ImGui::SameLine();
        ImGui::TextDisabled("off for timings: a vsynced GPU idles and clocks down");
        ImGui::Text("%u draws, %u rock groups", stats.draw_calls, stats.rock_groups_drawn);
        ImGui::Text("%u rocks, %.2f M triangles", stats.visible_asteroids, stats.triangles / 1e6);
        ImGui::PopID();
    }
    if (section("Quality")) {
        ImGui::Checkbox("High tier (F2)", &app.high);
        ImGui::SameLine();
        ImGui::TextDisabled(app.high ? "520k rocks" : "280k rocks");
        ImGui::PopID();
    }
    if (section("Anti-aliasing")) {
        ImGui::Checkbox("Temporal (F5)", &app.aa.temporal_aa);
        static constexpr const char* spatial[] = {"Off", "FXAA", "SMAA"};
        combo("Spatial (F9)", app.aa.spatial_aa, spatial);
        ImGui::PopID();
    }
    if (section("Belt")) {
        ImGui::Checkbox("Transmittance map (F3)", &app.belt.light_map);
        ImGui::Checkbox("Dust extinction (F4)", &app.belt.extinction);
        ImGui::Checkbox("Dust scattering (F11)", &app.belt_dust.enabled);
        ImGui::BeginDisabled(!app.belt_dust.enabled);
        ImGui::SliderFloat("Dust density", &app.belt_dust.density, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Dust brightness", &app.belt_dust.brightness, .1f, 8.f, "%.2f x",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Far-belt brightness", &app.belt_dust.far, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
        ImGui::ColorEdit3("Dust tint", app.belt_dust.tint, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Dust saturation", &app.belt_dust.saturation, 0.f, 3.f, "%.2f");
        if (ImGui::SmallButton("Reset dust"))
            app.belt_dust = {.enabled = app.belt_dust.enabled};
        ImGui::EndDisabled();
        static constexpr const char* cutoffs[] = {"Off", "1.2 px", "2.5 px", "4 px"};
        combo("Splat cut-off (F6)", app.belt.splat_mode, cutoffs);
        ImGui::Checkbox("Light splats in both cull passes (F7)", &app.belt.splat_light_twice);
        ImGui::Checkbox("Far-belt disc LOD",
                        &app.belt.disc); // off: the dust march and splats at every distance, as before the disc
        ImGui::SameLine();
        ImGui::TextDisabled("disc %.0f%%", stats.belt_lod * 100);
        ImGui::BeginDisabled(!app.belt.disc);
        ImGui::SliderFloat("Far-belt fade distance", &app.belt.lod_scale, .25f, 4.f, "%.2f x",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    if (section("Gas giant")) {
        ImGui::Checkbox("Flow map", &app.gas.flow); // off holds the deck still
        ImGui::BeginDisabled(!app.gas.flow);
        ImGui::SliderFloat("Wind speed", &app.gas.time_scale, 100.f, 20000.f, "%.0f x real",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Cycle", &app.gas.cycle, 4.f, 60.f, "%.0f s");
        ImGui::EndDisabled();
        ImGui::SliderFloat("Turbulence", &app.gas.turbulence, 0.f, 3.f, "%.2f x");
        ImGui::Checkbox("Close streaks", &app.gas.streaks); // per-pixel wind streaks where the filament map runs out
        ImGui::BeginDisabled(!app.gas.streaks);
        ImGui::SliderFloat("Streak strength", &app.gas.streak_strength, 0.f, 3.f, "%.2f x");
        ImGui::EndDisabled();
        ImGui::SliderFloat("Limb haze", &app.gas.haze, 0.f, .5f, "%.3f");
        ImGui::SliderFloat("Terminator softness", &app.gas.terminator, 0.f, .3f, "%.3f");
        ImGui::SliderFloat("Cloud relief", &app.gas.relief, 0.f, 20.f, "%.1f x");
        ImGui::SliderFloat("Lightning rate", &app.gas.lightning_rate, 0.f, 40.f, "%.0f /s");
        ImGui::SliderFloat("Lightning brightness", &app.gas.lightning, 0.f, 4.f, "%.2f x");
        ImGui::Checkbox("Polar cyclones", &app.gas.polar);
        ImGui::BeginDisabled(!app.gas.polar);
        ImGui::SliderFloat("Cap size", &app.gas.cap_size, .5f, 3.f, "%.2f x");
        ImGui::SliderFloat("Cap blend", &app.gas.cap_blend, .05f, 1.f, "%.2f");
        ImGui::SliderFloat("Cap opacity", &app.gas.cap_opacity, 0.f, 1.f, "%.2f");
        ImGui::EndDisabled();
        ImGui::Checkbox("Two decks", &app.gas.layers);
        ImGui::BeginDisabled(!app.gas.layers);
        ImGui::SliderFloat("Deck lift", &app.gas.layer_lift, 0.f, .006f, "%.4f");
        ImGui::SliderFloat("Deck shadow", &app.gas.layer_shadow, 0.f, 1.f, "%.2f");
        ImGui::EndDisabled();
        if (ImGui::SmallButton("Reset giant"))
            app.gas = {};
        ImGui::PopID();
    }
    if (section("Earth")) {
        ImGui::SliderFloat("Ocean roughness", &app.earth.ocean_roughness, .03f, .6f, "%.3f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Glint intensity", &app.earth.glint_intensity, .1f, 4.f, "%.2f x",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Sea patchiness", &app.earth.sea_patchiness, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Cloud shadow", &app.earth.cloud_shadow, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Cloud shadow softness", &app.earth.cloud_shadow_softness, 0.f, 4.f, "%.1f mips");
        ImGui::SliderFloat("Cloud opacity", &app.earth.cloud_opacity, 0.f, 1.f, "%.2f");
        if (ImGui::SmallButton("Reset Earth"))
            app.earth = {};
        ImGui::PopID();
    }
    if (section("Sun & lens")) {
        ImGui::SliderFloat("Glare", &app.sun.glare_intensity, 0.f, 4.f, "%.2f x");
        ImGui::SliderFloat("Ghosts", &app.sun.ghost_strength, 0.f, 4.f, "%.2f x");
        ImGui::SliderFloat("Starburst", &app.sun.starburst_strength, 0.f, 4.f, "%.2f x");
        ImGui::SliderInt("Starburst blades", &app.sun.starburst_blades, 0, 12);
        ImGui::SliderFloat("Disc radius", &app.sun.sun_disc_radius, .003f, .02f, "%.4f rad");
        ImGui::SliderFloat("Limb darkening", &app.sun.sun_limb_darkening, 0.f, 1.f, "%.2f");
        if (ImGui::SmallButton("Reset sun"))
            app.sun = {};
        ImGui::PopID();
    }
    if (section("Sky")) {
        ImGui::Checkbox("Catalogue stars",
                        &app.sky.catalogue_stars); // the Bright Star Catalogue; off keeps the procedural sky
        ImGui::BeginDisabled(!app.sky.catalogue_stars);
        ImGui::SliderFloat("Star brightness", &app.sky.star_brightness, 0.f, 4.f, "%.2f x",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Star colour", &app.sky.star_saturation, 0.f, 1.f, "%.2f");
        ImGui::EndDisabled();
        ImGui::Checkbox("Milky Way",
                        &app.sky.milky_way); // the fitted galactic band; off keeps the procedural band
        ImGui::BeginDisabled(!app.sky.milky_way);
        ImGui::SliderFloat("Milky Way brightness", &app.sky.milky_way_brightness, 0.f, 4.f, "%.2f x",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Milky Way contrast", &app.sky.milky_way_contrast, .5f, 3.f,
                           "%.2f"); // exponent about the bulge: the halo down, the core up
        {
            const char* modes[] = {"Splats", "Texture layers", "Original full resolution"};
            int choice = int(app.sky.galaxy_mode);
            if (ImGui::Combo("Galaxy background", &choice, modes, int(render::GalaxyMode::Count)))
                app.sky.galaxy_mode = render::GalaxyMode(choice);
        }
        if (app.sky.galaxy_mode == render::GalaxyMode::TextureLayers) {
            ImGui::SliderFloat("Global structure", &app.sky.galaxy_low, 0.f, 2.f);
            ImGui::SliderFloat("Coloured clouds", &app.sky.galaxy_clouds, 0.f, 2.f);
            ImGui::SliderFloat("Dark filaments", &app.sky.galaxy_filaments, 0.f, 1.f);
        }
        ImGui::BeginDisabled(app.sky.galaxy_mode != render::GalaxyMode::Splats);
        ImGui::SliderInt("Milky Way splats", &app.sky.milky_way_splats, 0,
                         4096); // the first n by energy; the file caps it
        {
            const char* labels[] = {"full", "half", "quarter"};
            int choice = app.sky.galaxy_resolution == render::GalaxyResolution::Quarter ? 2
                         : app.sky.galaxy_resolution == render::GalaxyResolution::Half  ? 1
                                                                                        : 0;
            if (ImGui::Combo("Splat pass resolution", &choice, labels,
                             3)) // quarter resolves a 0.6 degree splat at 1080p
                app.sky.galaxy_resolution = choice == 2   ? render::GalaxyResolution::Quarter
                                            : choice == 1 ? render::GalaxyResolution::Half
                                                          : render::GalaxyResolution::Full;
        }
        // The dust lanes' fBm: three octaves of value noise modulating the lanes' depth.
        ImGui::SliderFloat("Dust amplitude", &app.sky.dust_amplitude, 0.f, 3.f, "%.2f");
        ImGui::SliderFloat("Dust scale", &app.sky.dust_scale, .1f, 10.f, "%.2f deg",
                           ImGuiSliderFlags_Logarithmic); // the base octave's feature size
        ImGui::SliderFloat("Dust lacunarity", &app.sky.dust_lacunarity, 1.5f, 4.f, "%.2f");
        ImGui::SliderFloat("Dust gain", &app.sky.dust_gain, .2f, .9f, "%.2f");
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (ImGui::SmallButton("Reset sky"))
            app.sky = {};
        ImGui::PopID();
    }
    if (section("Post FX")) {
        ImGui::Checkbox("Bloom", &app.post.bloom);
        ImGui::SliderFloat("Bloom intensity", &app.post.bloom_intensity, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Bloom threshold", &app.post.bloom_threshold, 0.f, 3.f, "%.2f");
        ImGui::SliderFloat("Bloom knee", &app.post.bloom_knee, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Aberration", &app.post.aberration, 0.f, 4.f, "%.2f x");
        ImGui::SliderFloat("Vignette", &app.post.vignette, 0.f, .5f, "%.2f");
        ImGui::SliderFloat("Grain", &app.post.grain, 0.f, .05f, "%.3f");
        ImGui::SliderFloat("Black offset", &app.post.black_offset, 0.f, 1.f,
                           "%.2f"); // the neutral curve\'s flare subtraction; 1 as published
        ImGui::Checkbox("Motion streaks", &app.post.motion_streaks); // dust motes streaking past the moving camera
        ImGui::BeginDisabled(!app.post.motion_streaks);
        ImGui::SliderFloat("Streak intensity", &app.post.motion_streak_intensity, 0.f, 4.f, "%.2f x");
        ImGui::EndDisabled();
        if (ImGui::SmallButton("Reset post"))
            app.post = {};
        ImGui::PopID();
    }
    if (section("Tone")) {
        static constexpr const char* curves[] = {"ACES filmic", "AgX", "PBR Neutral"};
        combo("Curve (F8)", app.tone.tone_curve, curves);
        ImGui::SliderFloat("Exposure (+/-)", &app.tone.exposure, exposure_keys::range.min, exposure_keys::range.max,
                           "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("Auto exposure (X)", &app.tone.auto_exposure);
        ImGui::PopID();
    }
    if (section("Camera")) {
        for (std::size_t i = 0; i < bookmark_count; i++) {
            if (i % 3)
                ImGui::SameLine();
            if (ImGui::Button(bookmark_names[i], {104, 0})) {
                app.camera.set_bookmark(i, app.bodies);
                app.selected_body = unsigned(std::min(Camera::bookmark_body(i), app.bodies.size() - 1));
                app.camera.set_mode(CameraMode::Free);
            }
        }
        if (ImGui::Button("Tour (T)", {104, 0}))
            app.camera.toggle_tour();
        ImGui::SameLine();
        if (ImGui::Button("Orbit (O)", {104, 0}))
            app.camera.set_orbit_target(app.selected_body,
                                        app.bodies[app.selected_body].radius * control::orbit_zoom_radii);
        ImGui::SameLine();
        if (ImGui::Button("Free (F)", {104, 0}))
            app.camera.set_mode(CameraMode::Free);
        ImGui::Checkbox("Pause (Space)", &app.paused);
        ImGui::PopID();
    }
    if (section("Overlay")) {
        ImGui::Checkbox("HUD (F1)", &app.overlay);
        ImGui::SameLine();
        if (ImGui::Button("Capture (F10)"))
            app.capture_request = hotkey_capture_path;
        ImGui::TextDisabled("F12 hides this panel");
        ImGui::PopID();
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

} // namespace space::app
