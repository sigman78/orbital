#include "app/actions.hpp"
#include "app/ui.hpp"
#include "imgui.h"
#include "render/renderer.hpp"
#include <algorithm>
#include <vector>

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

void frame_controls(const render::Stats& stats, std::span<const float> recent_frame_ms, bool& vsync) {
    std::vector<float> sorted(recent_frame_ms.begin(), recent_frame_ms.end());
    std::sort(sorted.begin(), sorted.end());
    const float p95 = sorted.empty() ? 0
                                     : sorted[std::min(sorted.size() - 1, std::size_t(double(sorted.size()) * .95))];
    const float peak = sorted.empty() ? 1 : sorted.back();
    ImGui::Text("%d FPS   %.1f ms (p95 %.1f)", int(1000 / std::max(stats.frame_ms, .1f)), stats.frame_ms, p95);
    ImGui::PlotLines("##frame", recent_frame_ms.data(), int(recent_frame_ms.size()), 0, nullptr, 0, peak * 1.1f,
                     {-1, 60});
    ImGui::Text("GPU %.2f ms", stats.gpu_ms);
    ImGui::Indent();
    ImGui::Text("cull + shadow %.2f   surface %.2f", stats.shadow_ms, stats.surface_ms);
    ImGui::Text("  cull %.2f   body shadow %.2f", stats.cull_ms, stats.body_shadow_ms);
    ImGui::Text("  belt light %.2f   disc bake %.2f", stats.belt_light_ms, stats.belt_disc_ms);
    ImGui::Text("atmosphere %.2f   post %.2f", stats.atmosphere_ms, stats.post_ms);
    ImGui::Unindent();
    ImGui::Text("CPU prepare %.2f ms", stats.prepare_ms);
    ImGui::Checkbox("VSync", &vsync);
    ImGui::SameLine();
    ImGui::TextDisabled("off for timings: a vsynced GPU idles and clocks down");
    ImGui::Text("%u draws, %u rock groups", stats.draw_calls, stats.rock_groups_drawn);
    ImGui::Text("%u rocks, %.2f M triangles", stats.visible_asteroids, stats.triangles / 1e6);
}
void quality_controls(bool& high) {
    ImGui::Checkbox("High tier (F2)", &high);
    ImGui::SameLine();
    ImGui::TextDisabled(high ? "520k rocks" : "280k rocks");
}
void anti_aliasing_controls(render::AntiAliasingSettings& settings) {
    ImGui::Checkbox("Temporal (F5)", &settings.temporal_aa);
    static constexpr const char* spatial[] = {"Off", "FXAA", "SMAA"};
    combo("Spatial (F9)", settings.spatial_aa, spatial);
}
void belt_controls(render::BeltSettings& belt, render::BeltDustSettings& dust, float disc_weight) {
    ImGui::Checkbox("Transmittance map (F3)", &belt.light_map);
    ImGui::Checkbox("Dust extinction (F4)", &belt.extinction);
    ImGui::Checkbox("Dust scattering (F11)", &dust.enabled);
    ImGui::BeginDisabled(!dust.enabled);
    ImGui::SliderFloat("Dust density", &dust.density, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Dust brightness", &dust.brightness, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Far-belt brightness", &dust.far, .1f, 8.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::ColorEdit3("Dust tint", dust.tint, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float);
    ImGui::SliderFloat("Dust saturation", &dust.saturation, 0.f, 3.f, "%.2f");
    if (ImGui::SmallButton("Reset dust"))
        dust = {.enabled = dust.enabled};
    ImGui::EndDisabled();
    static constexpr const char* cutoffs[] = {"Off", "1.2 px", "2.5 px", "4 px"};
    combo("Splat cut-off (F6)", belt.splat_mode, cutoffs);
    ImGui::Checkbox("Light splats in both cull passes (F7)", &belt.splat_light_twice);
    ImGui::Checkbox("Far-belt disc LOD",
                    &belt.disc); // off: the dust march and splats at every distance, as before the disc
    ImGui::SameLine();
    ImGui::TextDisabled("disc %.0f%%", disc_weight * 100);
    ImGui::BeginDisabled(!belt.disc);
    ImGui::SliderFloat("Far-belt fade distance", &belt.lod_scale, .25f, 4.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::EndDisabled();
}
void gas_giant_controls(render::GasSettings& settings) {
    ImGui::Checkbox("Flow map", &settings.flow); // off holds the deck still
    ImGui::BeginDisabled(!settings.flow);
    ImGui::SliderFloat("Wind speed", &settings.time_scale, 100.f, 20000.f, "%.0f x real", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Cycle", &settings.cycle, 4.f, 60.f, "%.0f s");
    ImGui::EndDisabled();
    ImGui::SliderFloat("Turbulence", &settings.turbulence, 0.f, 3.f, "%.2f x");
    ImGui::Checkbox("Close streaks", &settings.streaks); // per-pixel wind streaks where the filament map runs out
    ImGui::BeginDisabled(!settings.streaks);
    ImGui::SliderFloat("Streak strength", &settings.streak_strength, 0.f, 3.f, "%.2f x");
    ImGui::EndDisabled();
    ImGui::SliderFloat("Limb haze", &settings.haze, 0.f, .5f, "%.3f");
    ImGui::SliderFloat("Terminator softness", &settings.terminator, 0.f, .3f, "%.3f");
    ImGui::SliderFloat("Cloud relief", &settings.relief, 0.f, 20.f, "%.1f x");
    ImGui::SliderFloat("Lightning rate", &settings.lightning_rate, 0.f, 40.f, "%.0f /s");
    ImGui::SliderFloat("Lightning brightness", &settings.lightning, 0.f, 4.f, "%.2f x");
    ImGui::Checkbox("Polar cyclones", &settings.polar);
    ImGui::BeginDisabled(!settings.polar);
    ImGui::SliderFloat("Cap size", &settings.cap_size, .5f, 3.f, "%.2f x");
    ImGui::SliderFloat("Cap blend", &settings.cap_blend, .05f, 1.f, "%.2f");
    ImGui::SliderFloat("Cap opacity", &settings.cap_opacity, 0.f, 1.f, "%.2f");
    ImGui::EndDisabled();
    ImGui::Checkbox("Two decks", &settings.layers);
    ImGui::BeginDisabled(!settings.layers);
    ImGui::SliderFloat("Deck lift", &settings.layer_lift, 0.f, .006f, "%.4f");
    ImGui::SliderFloat("Deck shadow", &settings.layer_shadow, 0.f, 1.f, "%.2f");
    ImGui::EndDisabled();
    if (ImGui::SmallButton("Reset giant"))
        settings = {};
}
void earth_controls(render::EarthSettings& settings) {
    ImGui::SliderFloat("Ocean roughness", &settings.ocean_roughness, .03f, .6f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Glint intensity", &settings.glint_intensity, .1f, 4.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Sea patchiness", &settings.sea_patchiness, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Cloud shadow", &settings.cloud_shadow, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Cloud shadow softness", &settings.cloud_shadow_softness, 0.f, 4.f, "%.1f mips");
    ImGui::SliderFloat("Cloud opacity", &settings.cloud_opacity, 0.f, 1.f, "%.2f");
    if (ImGui::SmallButton("Reset Earth"))
        settings = {};
}
void sun_lens_controls(render::SunSettings& settings) {
    ImGui::SliderFloat("Glare", &settings.glare_intensity, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Ghosts", &settings.ghost_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Starburst", &settings.starburst_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderInt("Starburst blades", &settings.starburst_blades, 0, 12);
    ImGui::SliderFloat("Disc radius", &settings.sun_disc_radius, .003f, .02f, "%.4f rad");
    ImGui::SliderFloat("Limb darkening", &settings.sun_limb_darkening, 0.f, 1.f, "%.2f");
    if (ImGui::SmallButton("Reset sun"))
        settings = {};
}
void sky_controls(render::SkySettings& settings) {
    ImGui::Checkbox("Catalogue stars",
                    &settings.catalogue_stars); // the Bright Star Catalogue; off keeps the procedural sky
    ImGui::BeginDisabled(!settings.catalogue_stars);
    ImGui::SliderFloat("Star brightness", &settings.star_brightness, 0.f, 4.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Star colour", &settings.star_saturation, 0.f, 1.f, "%.2f");
    ImGui::EndDisabled();
    ImGui::Checkbox("Milky Way",
                    &settings.milky_way); // the fitted galactic band; off keeps the procedural band
    ImGui::BeginDisabled(!settings.milky_way);
    ImGui::SliderFloat("Milky Way brightness", &settings.milky_way_brightness, 0.f, 4.f, "%.2f x",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Milky Way contrast", &settings.milky_way_contrast, .5f, 3.f,
                       "%.2f"); // exponent about the bulge: the halo down, the core up
    {
        const char* modes[] = {"Splats", "Texture layers", "Original full resolution"};
        int choice = int(settings.galaxy_mode);
        if (ImGui::Combo("Galaxy background", &choice, modes, int(render::GalaxyMode::Count)))
            settings.galaxy_mode = render::GalaxyMode(choice);
    }
    if (settings.galaxy_mode == render::GalaxyMode::TextureLayers) {
        ImGui::SliderFloat("Global structure", &settings.galaxy_low, 0.f, 2.f);
        ImGui::SliderFloat("Coloured clouds", &settings.galaxy_clouds, 0.f, 2.f);
        ImGui::SliderFloat("Dark filaments", &settings.galaxy_filaments, 0.f, 1.f);
    }
    ImGui::BeginDisabled(settings.galaxy_mode != render::GalaxyMode::Splats);
    ImGui::SliderInt("Milky Way splats", &settings.milky_way_splats, 0,
                     4096); // the first n by energy; the file caps it
    {
        const char* labels[] = {"full", "half", "quarter"};
        int choice = settings.galaxy_resolution == render::GalaxyResolution::Quarter ? 2
                     : settings.galaxy_resolution == render::GalaxyResolution::Half  ? 1
                                                                                     : 0;
        if (ImGui::Combo("Splat pass resolution", &choice, labels,
                         3)) // quarter resolves a 0.6 degree splat at 1080p
            settings.galaxy_resolution = choice == 2   ? render::GalaxyResolution::Quarter
                                         : choice == 1 ? render::GalaxyResolution::Half
                                                       : render::GalaxyResolution::Full;
    }
    // The dust lanes' fBm: three octaves of value noise modulating the lanes' depth.
    ImGui::SliderFloat("Dust amplitude", &settings.dust_amplitude, 0.f, 3.f, "%.2f");
    ImGui::SliderFloat("Dust scale", &settings.dust_scale, .1f, 10.f, "%.2f deg",
                       ImGuiSliderFlags_Logarithmic); // the base octave's feature size
    ImGui::SliderFloat("Dust lacunarity", &settings.dust_lacunarity, 1.5f, 4.f, "%.2f");
    ImGui::SliderFloat("Dust gain", &settings.dust_gain, .2f, .9f, "%.2f");
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (ImGui::SmallButton("Reset sky"))
        settings = {};
}
void post_fx_controls(render::PostSettings& settings) {
    ImGui::Checkbox("Bloom", &settings.bloom);
    ImGui::SliderFloat("Bloom intensity", &settings.bloom_intensity, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Bloom threshold", &settings.bloom_threshold, 0.f, 3.f, "%.2f");
    ImGui::SliderFloat("Bloom knee", &settings.bloom_knee, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Aberration", &settings.aberration, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Vignette", &settings.vignette, 0.f, .5f, "%.2f");
    ImGui::SliderFloat("Grain", &settings.grain, 0.f, .05f, "%.3f");
    ImGui::SliderFloat("Black offset", &settings.black_offset, 0.f, 1.f,
                       "%.2f"); // the neutral curve\'s flare subtraction; 1 as published
    ImGui::Checkbox("Motion streaks", &settings.motion_streaks); // dust motes streaking past the moving camera
    ImGui::BeginDisabled(!settings.motion_streaks);
    ImGui::SliderFloat("Streak intensity", &settings.motion_streak_intensity, 0.f, 4.f, "%.2f x");
    ImGui::EndDisabled();
    if (ImGui::SmallButton("Reset post"))
        settings = {};
}
void tone_controls(render::ToneSettings& settings) {
    static constexpr const char* curves[] = {"ACES filmic", "AgX", "PBR Neutral"};
    combo("Curve (F8)", settings.tone_curve, curves);
    ImGui::SliderFloat("Exposure (+/-)", &settings.exposure, exposure_keys::range.min, exposure_keys::range.max, "%.2f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Auto exposure (X)", &settings.auto_exposure);
}
void camera_controls(AppState& app) {
    for (std::size_t i = 0; i < bookmark_count; i++) {
        if (i % 3)
            ImGui::SameLine();
        if (ImGui::Button(bookmark_names[i], {104, 0})) {
            select_bookmark(app, i);
        }
    }
    if (ImGui::Button("Tour (T)", {104, 0}))
        toggle_tour(app);
    ImGui::SameLine();
    if (ImGui::Button("Orbit (O)", {104, 0}))
        orbit_selected(app);
    ImGui::SameLine();
    if (ImGui::Button("Free (F)", {104, 0}))
        free_camera(app);
    ImGui::Checkbox("Pause (Space)", &app.paused);
}
void overlay_controls(AppState& app) {
    ImGui::Checkbox("HUD (F1)", &app.overlay);
    ImGui::SameLine();
    if (ImGui::Button("Capture (F10)"))
        request_capture(app);
    ImGui::TextDisabled("F12 hides this panel");
}

} // namespace

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
        frame_controls(stats, recent_frame_ms, app.vsync);
        ImGui::PopID();
    }
    if (section("Quality")) {
        quality_controls(app.high);
        ImGui::PopID();
    }
    if (section("Anti-aliasing")) {
        anti_aliasing_controls(app.aa);
        ImGui::PopID();
    }
    if (section("Belt")) {
        belt_controls(app.belt, app.belt_dust, stats.belt_lod);
        ImGui::PopID();
    }
    if (section("Gas giant")) {
        gas_giant_controls(app.gas);
        ImGui::PopID();
    }
    if (section("Earth")) {
        earth_controls(app.earth);
        ImGui::PopID();
    }
    if (section("Sun & lens")) {
        sun_lens_controls(app.sun);
        ImGui::PopID();
    }
    if (section("Sky")) {
        sky_controls(app.sky);
        ImGui::PopID();
    }
    if (section("Post FX")) {
        post_fx_controls(app.post);
        ImGui::PopID();
    }
    if (section("Tone")) {
        tone_controls(app.tone);
        ImGui::PopID();
    }
    if (section("Camera")) {
        camera_controls(app);
        ImGui::PopID();
    }
    if (section("Overlay")) {
        overlay_controls(app);
        ImGui::PopID();
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

} // namespace space::app
