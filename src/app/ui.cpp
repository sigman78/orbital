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

void combo(const char* label, unsigned& value, std::span<const char* const> items) {
    int index = int(std::min<unsigned>(value, unsigned(items.size() - 1)));
    if (ImGui::Combo(label, &index, items.data(), int(items.size())))
        value = unsigned(index);
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
        ImGui::Checkbox("Temporal (F5)", &app.temporal_aa);
        static constexpr const char* spatial[] = {"Off", "FXAA", "SMAA"};
        combo("Spatial (F9)", app.spatial_aa, spatial);
        ImGui::PopID();
    }
    if (section("Belt")) {
        ImGui::Checkbox("Transmittance map (F3)", &app.belt_light_map);
        ImGui::Checkbox("Dust extinction (F4)", &app.belt_extinction);
        ImGui::Checkbox("Dust scattering (F11)", &app.belt_dust);
        ImGui::BeginDisabled(!app.belt_dust);
        ImGui::SliderFloat("Dust density", &app.dust_density, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Dust brightness", &app.dust_brightness, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Far-belt brightness", &app.dust_far, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
        ImGui::ColorEdit3("Dust tint", app.dust_tint, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Dust saturation", &app.dust_saturation, 0.f, 3.f, "%.2f");
        if (ImGui::SmallButton("Reset dust")) {
            app.dust_density = app.dust_brightness = app.dust_far = app.dust_saturation = 1;
            app.dust_tint[0] = app.dust_tint[1] = app.dust_tint[2] = 1;
        }
        ImGui::EndDisabled();
        static constexpr const char* cutoffs[] = {"Off", "1.2 px", "2.5 px", "4 px"};
        combo("Splat cut-off (F6)", app.splat_mode, cutoffs);
        ImGui::Checkbox("Light splats in both cull passes (F7)", &app.splat_light_twice);
        ImGui::Checkbox("Far-belt disc LOD",
                        &app.belt_disc); // off: the dust march and splats at every distance, as before the disc
        ImGui::SameLine();
        ImGui::TextDisabled("disc %.0f%%", stats.belt_lod * 100);
        ImGui::BeginDisabled(!app.belt_disc);
        ImGui::SliderFloat("Far-belt fade distance", &app.belt_lod_scale, .25f, 4.f, "%.2f x",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    if (section("Gas giant")) {
        ImGui::Checkbox("Flow map", &app.gas_flow); // off holds the deck still
        ImGui::BeginDisabled(!app.gas_flow);
        ImGui::SliderFloat("Wind speed", &app.gas_time_scale, 100.f, 20000.f, "%.0f x real",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Cycle", &app.gas_cycle, 4.f, 60.f, "%.0f s");
        ImGui::EndDisabled();
        ImGui::SliderFloat("Turbulence", &app.gas_turbulence, 0.f, 3.f, "%.2f x");
        ImGui::Checkbox("Close streaks", &app.gas_streaks); // per-pixel wind streaks where the filament map runs out
        ImGui::BeginDisabled(!app.gas_streaks);
        ImGui::SliderFloat("Streak strength", &app.gas_streak_strength, 0.f, 3.f, "%.2f x");
        ImGui::EndDisabled();
        ImGui::SliderFloat("Limb haze", &app.gas_haze, 0.f, .5f, "%.3f");
        ImGui::SliderFloat("Terminator softness", &app.gas_terminator, 0.f, .3f, "%.3f");
        ImGui::SliderFloat("Cloud relief", &app.gas_relief, 0.f, 20.f, "%.1f x");
        ImGui::SliderFloat("Lightning rate", &app.gas_lightning_rate, 0.f, 40.f, "%.0f /s");
        ImGui::SliderFloat("Lightning brightness", &app.gas_lightning, 0.f, 4.f, "%.2f x");
        ImGui::Checkbox("Polar cyclones", &app.gas_polar);
        ImGui::BeginDisabled(!app.gas_polar);
        ImGui::SliderFloat("Cap size", &app.gas_cap_size, .5f, 3.f, "%.2f x");
        ImGui::SliderFloat("Cap blend", &app.gas_cap_blend, .05f, 1.f, "%.2f");
        ImGui::SliderFloat("Cap opacity", &app.gas_cap_opacity, 0.f, 1.f, "%.2f");
        ImGui::EndDisabled();
        ImGui::Checkbox("Two decks", &app.gas_layers);
        ImGui::BeginDisabled(!app.gas_layers);
        ImGui::SliderFloat("Deck lift", &app.gas_layer_lift, 0.f, .006f, "%.4f");
        ImGui::SliderFloat("Deck shadow", &app.gas_layer_shadow, 0.f, 1.f, "%.2f");
        ImGui::EndDisabled();
        if (ImGui::SmallButton("Reset giant")) {
            app.gas_flow = true;
            app.gas_time_scale = 1500.f;
            app.gas_cycle = 12.f;
            app.gas_turbulence = 1.f;
            app.gas_haze = .08f;
            app.gas_terminator = .08f;
            app.gas_relief = 6.f;
            app.gas_lightning_rate = 4.f;
            app.gas_lightning = 1.f;
            app.gas_polar = true;
            app.gas_cap_size = 1.5f;
            app.gas_cap_blend = .5f;
            app.gas_cap_opacity = 1.f;
            app.gas_layers = true;
            app.gas_layer_lift = .0015f;
            app.gas_layer_shadow = .3f;
            app.gas_streaks = true;
            app.gas_streak_strength = 1.f;
        }
        ImGui::PopID();
    }
    if (section("Earth")) {
        ImGui::SliderFloat("Ocean roughness", &app.ocean_roughness, .03f, .6f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Glint intensity", &app.glint_intensity, .1f, 4.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Sea patchiness", &app.sea_patchiness, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Cloud shadow", &app.cloud_shadow, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Cloud shadow softness", &app.cloud_shadow_softness, 0.f, 4.f, "%.1f mips");
        ImGui::SliderFloat("Cloud opacity", &app.cloud_opacity, 0.f, 1.f, "%.2f");
        if (ImGui::SmallButton("Reset Earth")) {
            app.ocean_roughness = .18f;
            app.glint_intensity = 1.f;
            app.sea_patchiness = .5f;
            app.cloud_shadow = .6f;
            app.cloud_shadow_softness = 1.5f;
            app.cloud_opacity = .88f;
        }
        ImGui::PopID();
    }
    if (section("Sun & lens")) {
        ImGui::SliderFloat("Glare", &app.glare_intensity, 0.f, 4.f, "%.2f x");
        ImGui::SliderFloat("Ghosts", &app.ghost_strength, 0.f, 4.f, "%.2f x");
        ImGui::SliderFloat("Starburst", &app.starburst_strength, 0.f, 4.f, "%.2f x");
        ImGui::SliderInt("Starburst blades", &app.starburst_blades, 0, 12);
        ImGui::SliderFloat("Disc radius", &app.sun_disc_radius, .003f, .02f, "%.4f rad");
        ImGui::SliderFloat("Limb darkening", &app.sun_limb_darkening, 0.f, 1.f, "%.2f");
        if (ImGui::SmallButton("Reset sun")) {
            app.glare_intensity = app.ghost_strength = app.starburst_strength = 1.f;
            app.starburst_blades = 6;
            app.sun_disc_radius = .007f;
            app.sun_limb_darkening = .6f;
        }
        ImGui::PopID();
    }
    if (section("Sky")) {
        ImGui::Checkbox("Catalogue stars",
                        &app.catalogue_stars); // the Bright Star Catalogue; off keeps the procedural sky
        ImGui::BeginDisabled(!app.catalogue_stars);
        ImGui::SliderFloat("Star brightness", &app.star_brightness, 0.f, 4.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Star colour", &app.star_saturation, 0.f, 1.f, "%.2f");
        ImGui::EndDisabled();
        if (ImGui::SmallButton("Reset sky")) {
            app.catalogue_stars = true;
            app.star_brightness = 2.f;
            app.star_saturation = .5f;
        }
        ImGui::PopID();
    }
    if (section("Post FX")) {
        ImGui::Checkbox("Bloom", &app.bloom);
        ImGui::SliderFloat("Bloom intensity", &app.bloom_intensity, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Bloom threshold", &app.bloom_threshold, 0.f, 3.f, "%.2f");
        ImGui::SliderFloat("Bloom knee", &app.bloom_knee, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Aberration", &app.aberration, 0.f, 4.f, "%.2f x");
        ImGui::SliderFloat("Vignette", &app.vignette, 0.f, .5f, "%.2f");
        ImGui::SliderFloat("Grain", &app.grain, 0.f, .05f, "%.3f");
        ImGui::Checkbox("Motion streaks", &app.motion_streaks); // dust motes streaking past the moving camera
        ImGui::BeginDisabled(!app.motion_streaks);
        ImGui::SliderFloat("Streak intensity", &app.motion_streak_intensity, 0.f, 4.f, "%.2f x");
        ImGui::EndDisabled();
        if (ImGui::SmallButton("Reset post")) {
            app.bloom = true;
            app.bloom_intensity = .24f;
            app.bloom_threshold = .85f;
            app.bloom_knee = .5f;
            app.aberration = 1.f;
            app.vignette = .17f;
            app.grain = .010f;
            app.motion_streaks = true;
            app.motion_streak_intensity = 1.f;
        }
        ImGui::PopID();
    }
    if (section("Tone")) {
        static constexpr const char* curves[] = {"ACES filmic", "AgX", "PBR Neutral"};
        combo("Curve (F8)", app.tone_curve, curves);
        ImGui::SliderFloat("Exposure (+/-)", &app.exposure, exposure_keys::range.min, exposure_keys::range.max, "%.2f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("Auto exposure (X)", &app.auto_exposure);
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
