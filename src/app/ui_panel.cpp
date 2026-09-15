#include "app/actions.hpp"
#include "app/ui.hpp"
#include "imgui.h"
#include "render/renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace space::app {
namespace {

constexpr float panel_width = 360;
constexpr const char* bookmark_names[] = {"Earth", "Jupiter", "Moon", "Mars", "Dawn", "Belt"};
static_assert(std::size(bookmark_names) == bookmark_count);

// A collapsing section with its own ID scope: headers push none, so labels
// could otherwise collide across sections (the "Belt" header and button did).
bool section(const char* title, bool default_open = false) {
    const bool open = ImGui::CollapsingHeader(title, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (open)
        ImGui::PushID(title);
    return open;
}

template <class Enum> void combo(const char* label, Enum& value, std::span<const char* const> items) {
    int index = int(std::min<unsigned>(unsigned(value), unsigned(items.size() - 1)));
    if (ImGui::Combo(label, &index, items.data(), int(items.size())))
        value = Enum(index);
}

void gpu_time_bar(const render::Stats& stats) {
    struct Segment {
        const char* label;
        float ms;
        ImU32 color;
    };
    std::array segments{
        Segment{.label = "Culling", .ms = stats.cull_ms, .color = IM_COL32(86, 180, 233, 255)},
        Segment{.label = "Body shadows", .ms = stats.body_shadow_ms, .color = IM_COL32(130, 120, 210, 255)},
        Segment{.label = "Belt light", .ms = stats.belt_light_ms, .color = IM_COL32(230, 159, 0, 255)},
        Segment{.label = "Belt discs", .ms = stats.belt_disc_ms, .color = IM_COL32(240, 228, 66, 255)},
        Segment{.label = "Surface", .ms = stats.surface_ms, .color = IM_COL32(0, 158, 115, 255)},
        Segment{.label = "Atmosphere", .ms = stats.atmosphere_ms, .color = IM_COL32(204, 121, 167, 255)},
        Segment{.label = "Post FX", .ms = stats.post_ms, .color = IM_COL32(213, 94, 0, 255)},
        Segment{.label = "Other", .ms = 0, .color = IM_COL32(140, 145, 155, 255)}};
    float sum = 0;
    for (auto& segment : segments) {
        segment.ms = std::max(segment.ms, 0.f);
        sum += segment.ms;
    }
    segments.back().ms = std::max(stats.gpu_ms - sum, 0.f);
    const float total = std::max(stats.gpu_ms, sum);
    if (total <= 0) {
        ImGui::TextDisabled("Waiting for GPU timings...");
        return;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{ImGui::GetContentRegionAvail().x, 18};
    ImGui::InvisibleButton("##gpu-time", size);
    const bool hovered = ImGui::IsItemHovered();
    float x = origin.x;
    for (const auto& segment : segments) {
        const float end = x + size.x * segment.ms / total;
        ImGui::GetWindowDrawList()->AddRectFilled({x, origin.y}, {end, origin.y + size.y}, segment.color);
        if (hovered && ImGui::GetIO().MousePos.x >= x && ImGui::GetIO().MousePos.x < end)
            ImGui::SetTooltip("%s: %.2f ms (%.1f%%)", segment.label, segment.ms, 100 * segment.ms / total);
        x = end;
    }
    if (ImGui::BeginTable("##gpu-passes", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (const auto& segment : segments) {
            ImGui::TableNextColumn();
            const auto swatch = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled({swatch.x, swatch.y + 3}, {swatch.x + 9, swatch.y + 12},
                                                      segment.color);
            ImGui::Dummy({9, 12});
            ImGui::SameLine();
            ImGui::TextUnformatted(segment.label);
            ImGui::SameLine(0, ImGui::CalcTextSize(" ").x);
            char reading[64];
            std::snprintf(reading, sizeof(reading), "%.2f ms", segment.ms);
            const float padding = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(reading).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(padding, 0.f));
            ImGui::TextColored({.55f, .8f, 1.f, 1.f}, "%s", reading);
        }
        ImGui::EndTable();
    }
    if (sum > stats.gpu_ms)
        ImGui::TextDisabled("Shares normalized to summed pass timings");
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
    ImGui::TextDisabled("Timings: 0.5 s average | graph: raw frames");
    ImGui::Text("GPU %.2f ms", stats.gpu_ms);
    gpu_time_bar(stats);
    ImGui::Text("Cull + shadows %.2f ms", stats.shadow_ms);
    ImGui::Text("CPU prepare %.2f ms", stats.prepare_ms);
    ImGui::Checkbox("VSync", &vsync);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Disable VSync for performance comparisons: a waiting GPU may clock down.");
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
    ImGui::SliderFloat("Ambient fill", &settings.ambient_fill, 0.f, 2.f, "%.2f x"); // starlight on shadow sides
    ImGui::SliderFloat("Glare", &settings.glare_intensity, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Starburst", &settings.starburst_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderInt("Starburst blades", &settings.starburst_blades, 0, 12);
    ImGui::SliderFloat("Disc radius", &settings.sun_disc_radius, .003f, .02f, "%.4f rad");
    ImGui::SliderFloat("Limb darkening", &settings.sun_limb_darkening, 0.f, 1.f, "%.2f");
    // The flare stack: major elements with the sun in frame, ghosts as it nears and leaves the edge.
    ImGui::Checkbox("Lens flare", &settings.lens_flare); // off skips the stack pass and every element; the glare stays
    ImGui::BeginDisabled(!settings.lens_flare);
    ImGui::TextDisabled("Flare stack");
    ImGui::SliderFloat("Main ring", &settings.ring_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Mini crescent", &settings.mini_crescent_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Big crescent", &settings.crescent_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Streak", &settings.streak_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Ghosts", &settings.ghost_strength, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Ghost spread", &settings.ghost_spread, .5f, 2.f, "%.2f x");
    ImGui::SliderFloat("Ghost size", &settings.ghost_size, .5f, 2.f, "%.2f x");
    ImGui::SliderFloat("Saturation", &settings.flare_saturation, 0.f, 2.f, "%.2f x");
    {
        const char* labels[] = {"half", "quarter", "eighth"};
        int choice = settings.flare_resolution == render::FlareResolution::Eighth ? 2
                     : settings.flare_resolution == render::FlareResolution::Half ? 0
                                                                                  : 1;
        if (ImGui::Combo("Stack resolution", &choice, labels, 3)) // coarser softens the stack, as defocus
            settings.flare_resolution = choice == 2   ? render::FlareResolution::Eighth
                                        : choice == 0 ? render::FlareResolution::Half
                                                      : render::FlareResolution::Quarter;
    }
    ImGui::EndDisabled();
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
                       "%.2f");                            // the neutral curve\'s flare subtraction; 1 as published
    ImGui::Checkbox("Dirty glass", &settings.dirty_glass); // a film on a convex pane: blurs, brightens at grazing sun
    ImGui::BeginDisabled(!settings.dirty_glass);
    ImGui::SliderFloat("Dirt light-up", &settings.dirt_light, 0.f, 4.f, "%.2f x");
    ImGui::SliderFloat("Dirt blur", &settings.dirt_blur, 0.f, 2.f, "%.2f x");
    ImGui::EndDisabled();
    ImGui::Checkbox("Motion streaks", &settings.motion_streaks); // dust motes streaking past the moving camera
    ImGui::BeginDisabled(!settings.motion_streaks);
    ImGui::SliderFloat("Streak intensity", &settings.motion_streak_intensity, 0.f, 4.f, "%.2f x");
    ImGui::EndDisabled();
    if (ImGui::SmallButton("Reset post"))
        settings = {};
}
void tone_controls(render::ToneSettings& settings, const render::Stats& stats, const render::DisplaySettings& display) {
    const auto& exposure = stats.exposure;
    static constexpr const char* curves[] = {"ACES filmic", "AgX", "PBR Neutral"};
    combo("Curve (F8)", settings.tone_curve, curves);
    ImGui::SliderFloat("Exposure (+/-)", &settings.exposure, exposure_keys::range.min, exposure_keys::range.max, "%.2f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Auto exposure (X)", &settings.auto_exposure);
    ImGui::BeginDisabled(!settings.auto_exposure);
    ImGui::SliderFloat("Meter key", &settings.meter_key, .005f, .5f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Highlight bias", &settings.highlight_bias, 0.f, .5f, "%.2f");
    ImGui::SliderFloat("Adaptation strength", &settings.adapt_strength, 0.f, 1.f,
                       "%.2f"); // in stops; the range still clamps
    ImGui::SliderFloat("Adaptation min", &settings.adapt_min, .05f, 1.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Adaptation max", &settings.adapt_max, 1.f, 32.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::EndDisabled();
    // The active curve's exposure trim; each curve keeps its own value.
    ImGui::SliderFloat("Curve trim", &settings.curve_trim[std::min(unsigned(settings.tone_curve), 2u)], .1f, 2.f,
                       "%.2f x");
    // The swapchain's output; the HDR pairs are offered only with the OS presenting in HDR.
    static constexpr const char* outputs[] = {"SDR (8-bit sRGB)", "HDR scRGB (16-bit float)", "HDR10 (10-bit PQ)"};
    combo("Output", settings.hdr_output, outputs);
    if (stats.hdr_unsupported)
        ImGui::TextDisabled("Not offered by the display; is HDR on in the OS?");
    if (display.hdr)
        ImGui::Text("Display: HDR, %.0f to %.0f nits, SDR white %.0f", display.min_nits, display.max_nits,
                    display.sdr_white_nits);
    else
        ImGui::TextDisabled("Display: SDR desktop");
    ImGui::BeginDisabled(settings.hdr_output == render::HdrOutput::Off);
    ImGui::SliderFloat("Paper white", &settings.paper_white_nits, 80.f, 400.f, "%.0f nits");
    ImGui::SliderFloat("Peak brightness", &settings.peak_nits, 200.f, 4000.f, "%.0f nits",
                       ImGuiSliderFlags_Logarithmic);
    if (display.hdr && display.max_nits > 0 && ImGui::SmallButton("From display")) {
        settings.peak_nits = display.max_nits;
        if (display.sdr_white_nits > 0)
            settings.paper_white_nits = display.sdr_white_nits;
    }
    if (!stats.hdr_metadata)
        ImGui::TextDisabled("No HDR metadata path on this device");
    ImGui::EndDisabled();
    ImGui::Spacing();
    if (!exposure.ready) {
        ImGui::TextDisabled("Waiting for HDR measurement...");
    } else {
        if (exposure.has_samples)
            ImGui::Text("HDR luminance: %.4g", exposure.luminance);
        else
            ImGui::TextDisabled("HDR luminance: below metering threshold");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Center-weighted geometric mean before exposure and bloom.\nVery dark samples do not "
                              "contribute; this is scene-linear luminance, not nits.");
        ImGui::Text("HDR peak (sampled): %.4g", exposure.peak_luminance);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Brightest averaged cell in the 16 x 16 meter.\nThis is not the brightest full-resolution pixel.");
        ImGui::Text("Adaptation: %.3f x -> %.3f x%s", exposure.adapted, exposure.target,
                    exposure.limited ? " (limited)" : "");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Current automatic multiplier -> clamped target.\nMeter updates every 16 frames; "
                              "adaptation follows over time.");
    }
    if (!exposure.automatic)
        ImGui::TextDisabled("Auto adjustment bypassed");
    ImGui::Text("Applied exposure: %.3f x (%+.2f stops)", exposure.applied,
                std::log2(std::max(exposure.applied, 1e-6f)));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Manual exposure x active auto adjustment x tone-curve calibration.\nStops are relative to a "
                          "multiplier of 1; bloom is combined before this gain.");
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
    if (section("Frame", true)) {
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
        tone_controls(app.tone, stats, app.display);
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
