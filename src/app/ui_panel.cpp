#include "app/actions.hpp"
#include "app/ui.hpp"
#include "imgui.h"
#include "render/renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace space::app {
namespace {

constexpr float panel_width = 360;
constexpr const char* bookmark_names[] = {"Earth", "Jupiter",     "Moon",         "Mars",         "Dawn",
                                          "Belt",  "Dust shadow", "Dust grazing", "Minor planet", "Minor planet close"};
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

// The pass timings: the groups across the bar's upper half, and in the lower half of
// each group its children in proportion, alternating light and dark, with the group's
// remainder (its barriers and transitions) as a gap; the tooltip lists the numbers.
void gpu_time_bar(const render::FrameStats& stats) {
    using render::GpuPass;
    struct Child {
        const char* label;
        float ms;
    };
    struct Segment {
        const char* label;
        float ms;
        ImU32 color;
        std::vector<Child> children;
    };
    // The bar's segments: the four cull-and-shadow passes on their own and the three
    // shaded groups; each group's children come from the pass table.
    struct Bar {
        GpuPass pass;
        ImU32 color;
    };
    static constexpr Bar bars[] = {
        {GpuPass::Culling, IM_COL32(86, 180, 233, 255)},  {GpuPass::BodyShadows, IM_COL32(130, 120, 210, 255)},
        {GpuPass::BeltLight, IM_COL32(230, 159, 0, 255)}, {GpuPass::BeltDiscs, IM_COL32(240, 228, 66, 255)},
        {GpuPass::Surface, IM_COL32(0, 158, 115, 255)},   {GpuPass::Atmosphere, IM_COL32(204, 121, 167, 255)},
        {GpuPass::Post, IM_COL32(213, 94, 0, 255)}};
    std::vector<Segment> segments;
    segments.reserve(std::size(bars) + 1);
    for (const Bar& bar : bars) {
        Segment segment{.label = render::pass_info(bar.pass).label, .ms = stats.gpu[bar.pass], .color = bar.color};
        for (std::size_t i = 0; i < render::gpu_pass_count; i++)
            if (render::gpu_pass_info[i].parent == bar.pass && GpuPass(i) != bar.pass)
                segment.children.push_back({render::gpu_pass_info[i].label, stats.gpu.ms[i]});
        segments.push_back(std::move(segment));
    }
    segments.push_back({.label = "Other", .ms = 0, .color = IM_COL32(140, 145, 155, 255)});
    const float gpu_ms = stats.gpu[GpuPass::Frame];
    float sum = 0;
    for (auto& segment : segments) {
        segment.ms = std::max(segment.ms, 0.f);
        sum += segment.ms;
    }
    segments.back().ms = std::max(gpu_ms - sum, 0.f);
    const float total = std::max(gpu_ms, sum);
    if (total <= 0) {
        ImGui::TextDisabled("Waiting for GPU timings...");
        return;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{ImGui::GetContentRegionAvail().x, 18};
    const float split = origin.y + size.y * .5f;
    ImGui::InvisibleButton("##gpu-time", size);
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    float x = origin.x;
    for (const auto& segment : segments) {
        const float end = x + size.x * segment.ms / total;
        const float bottom = segment.children.empty() ? origin.y + size.y : split;
        draw->AddRectFilled({x, origin.y}, {end, bottom}, segment.color);
        if (!segment.children.empty()) {
            // Children in proportion to the group, the remainder left as a darker gap.
            draw->AddRectFilled({x, split}, {end, origin.y + size.y}, IM_COL32(0, 0, 0, 120));
            float cx = x;
            for (std::size_t i = 0; i < segment.children.size(); i++) {
                const float width = segment.ms > 0 ? (end - x) * std::max(segment.children[i].ms, 0.f) / segment.ms : 0;
                const ImU32 shade = i % 2 ? IM_COL32(0, 0, 0, 60) : IM_COL32(255, 255, 255, 70);
                draw->AddRectFilled({cx, split}, {std::min(cx + width, end), origin.y + size.y}, segment.color);
                draw->AddRectFilled({cx, split}, {std::min(cx + width, end), origin.y + size.y}, shade);
                cx += width;
            }
        }
        if (hovered && ImGui::GetIO().MousePos.x >= x && ImGui::GetIO().MousePos.x < end) {
            ImGui::BeginTooltip();
            ImGui::Text("%s  %.2f ms  %.1f%%", segment.label, segment.ms, 100 * segment.ms / total);
            // Each child: milliseconds, share of the group, share of the frame; the
            // remainder is the group's barriers. Alternating row backgrounds, as a sheet.
            const float group = std::max(segment.ms, 1e-6f);
            if (!segment.children.empty() &&
                ImGui::BeginTable("##children", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                const auto row = [&](const char* label, float ms, bool dim) {
                    ImGui::TableNextRow();
                    if (dim)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(label);
                    ImGui::TableNextColumn();
                    ImGui::Text("%8.3f ms", ms);
                    ImGui::TableNextColumn();
                    ImGui::Text("%5.1f%%", 100 * ms / group);
                    ImGui::TableNextColumn();
                    ImGui::Text("%5.1f%%", 100 * ms / total);
                    if (dim)
                        ImGui::PopStyleColor();
                };
                ImGui::TableNextRow();
                for (const char* heading : {"", "", "group", "frame"}) {
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", heading);
                }
                float accounted = 0;
                for (const auto& child : segment.children) {
                    row(child.label, std::max(child.ms, 0.f), false);
                    accounted += std::max(child.ms, 0.f);
                }
                row("barriers, other", std::max(segment.ms - accounted, 0.f), true);
                ImGui::EndTable();
            }
            ImGui::EndTooltip();
        }
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
    if (sum > gpu_ms)
        ImGui::TextDisabled("Shares normalized to summed pass timings");
}

// GPU memory by resource type, as the pass timings are shown: a bar of the shares
// and a table of the pools with their allocation counts and, for the mapped heap,
// how much of it is in use.
void memory_bar(const render::MemoryStats& memory) {
    struct Pool {
        const char* label;
        const render::MemoryPool* pool;
        ImU32 color;
    };
    const std::array pools{
        Pool{.label = "Frame targets", .pool = &memory.frame_targets, .color = IM_COL32(86, 180, 233, 255)},
        Pool{.label = "Fixed targets", .pool = &memory.fixed_targets, .color = IM_COL32(130, 120, 210, 255)},
        Pool{.label = "Materials", .pool = &memory.materials, .color = IM_COL32(0, 158, 115, 255)},
        Pool{.label = "Static records", .pool = &memory.static_data, .color = IM_COL32(240, 228, 66, 255)},
        Pool{.label = "Mapped heap", .pool = &memory.mapped, .color = IM_COL32(230, 159, 0, 255)},
        Pool{.label = "Device buffers", .pool = &memory.device_buffers, .color = IM_COL32(213, 94, 0, 255)},
        Pool{.label = "Readback", .pool = &memory.readback, .color = IM_COL32(204, 121, 167, 255)},
        Pool{.label = "Tile arrays", .pool = &memory.tile_arrays, .color = IM_COL32(140, 200, 120, 255)}};
    const double total = double(memory.total());
    const auto mib = [](std::uint64_t bytes) { return double(bytes) / (1024.0 * 1024.0); };
    ImGui::Text("GPU memory %.1f MiB", mib(memory.total()));
    if (total <= 0) {
        ImGui::TextDisabled("No allocations yet");
        return;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{ImGui::GetContentRegionAvail().x, 18};
    ImGui::InvisibleButton("##gpu-memory", size);
    const bool hovered = ImGui::IsItemHovered();
    float x = origin.x;
    for (const auto& entry : pools) {
        const float end = x + float(size.x * double(entry.pool->bytes) / total);
        ImGui::GetWindowDrawList()->AddRectFilled({x, origin.y}, {end, origin.y + size.y}, entry.color);
        if (entry.pool->used < entry.pool->bytes) // the unused part of a suballocated heap, hatched darker
            ImGui::GetWindowDrawList()->AddRectFilled(
                {x + float((end - x) * double(entry.pool->used) / double(entry.pool->bytes)), origin.y + 9},
                {end, origin.y + size.y}, IM_COL32(0, 0, 0, 110));
        if (hovered && ImGui::GetIO().MousePos.x >= x && ImGui::GetIO().MousePos.x < end)
            ImGui::SetTooltip("%s: %.1f MiB (%.1f%%), %u allocation%s", entry.label, mib(entry.pool->bytes),
                              100.0 * double(entry.pool->bytes) / total, entry.pool->count,
                              entry.pool->count == 1 ? "" : "s");
        x = end;
    }
    if (ImGui::BeginTable("##gpu-memory-pools", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (const auto& entry : pools) {
            ImGui::TableNextColumn();
            const auto swatch = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled({swatch.x, swatch.y + 3}, {swatch.x + 9, swatch.y + 12},
                                                      entry.color);
            ImGui::Dummy({9, 12});
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.label);
            ImGui::SameLine(0, ImGui::CalcTextSize(" ").x);
            char reading[64];
            if (entry.pool->used < entry.pool->bytes)
                std::snprintf(reading, sizeof(reading), "%.1f/%.1f MiB", mib(entry.pool->used), mib(entry.pool->bytes));
            else
                std::snprintf(reading, sizeof(reading), "%.1f MiB x%u", mib(entry.pool->bytes), entry.pool->count);
            const float padding = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(reading).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(padding, 0.f));
            ImGui::TextColored({.55f, .8f, 1.f, 1.f}, "%s", reading);
        }
        ImGui::EndTable();
    }
}

void frame_controls(const SmoothedStats& smoothed, const FrameHistory& history, bool& vsync) {
    const render::FrameStats& stats = smoothed.frame;
    const auto raw = history.values();
    ImGui::Text("%d FPS   %.1f ms (p95 %.1f)", int(1000 / std::max(smoothed.frame_ms, .1f)), smoothed.frame_ms,
                history.percentile(.95f));
    ImGui::PlotLines("##frame", raw.data(), int(raw.size()), history.offset(), nullptr, 0,
                     std::max(history.peak(), 1.f) * 1.1f, {-1, 60});
    ImGui::Text("Time   %.2f / %.2f / %.2f ms", stats.draw_ms, stats.prepare_ms, stats.gpu[render::GpuPass::Frame]);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("CPU draw (including GPU wait) / CPU prepare / GPU frame.\n"
                          "Timings are smoothed over 0.5 s; the graph shows raw frame times.");
    gpu_time_bar(stats);
    ImGui::Checkbox("VSync", &vsync);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Disable VSync for performance comparisons: a waiting GPU may clock down.");
    ImGui::Text("Scene  %u / %u / %u", stats.draw_calls, stats.bodies_drawn, stats.rock_groups_drawn);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Draw calls / bodies / rock groups");
    ImGui::Text("Mesh   %u / %.2f M", stats.visible_asteroids, stats.triangles / 1e6);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Visible rocks / triangles (millions)");
}
// The near tier's quadtree, drawn as the cube unwrapped: six faces in a cross, each patch the
// cell it occupies on its face, each drawn quadrant a square of that cell. A quadrant missing
// from a patch is a quadrant its child draws, or one nothing does -- which is what the map is
// for. Hue is the level, brightness how far the patch stands from its own shape: bright is its
// own, dark is its parent's, and a new patch reads white.
void patch_map(const render::Stats::PatchMap& map) {
    const std::span<const render::PatchView> patches = map.patches;
    // The six faces in index order, three by two. A cross reads as a cube but spends half the
    // panel on squares that hold nothing, and a face here is worth more than the adjacency: the
    // same width gives 118 pixels a face against 85, and the labels say which is which.
    static constexpr const char* labels[] = {"+x", "-x", "+y", "-y", "+z", "-z"};
    constexpr float gap = 1;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float side = std::floor((avail - 2 * gap) / 3);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##patch-map", {3 * side + 2 * gap, 2 * side + gap});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hue is the patch level, dark is morphed to the parent's shape, washed out is a\n"
                          "tile still fading in. A gap inside a patch is a quadrant its child draws, or\n"
                          "one nothing draws. The cross is the camera.");
    auto* draw = ImGui::GetWindowDrawList();
    const auto corner = [&](unsigned face) {
        return ImVec2{origin.x + (face % 3) * (side + gap), origin.y + (face / 3) * (side + gap)};
    };
    for (unsigned face = 0; face < 6; face++) {
        const ImVec2 at = corner(face);
        draw->AddRectFilled(at, {at.x + side, at.y + side}, IM_COL32(18, 20, 24, 255));
        draw->AddText({at.x + 2, at.y + 1}, IM_COL32(96, 104, 116, 255), labels[face]);
    }
    unsigned drawn_quadrants = 0, deepest = 0;
    for (const render::PatchView& patch : patches) {
        if (patch.face >= 6)
            continue;
        deepest = std::max(deepest, unsigned(patch.level));
        const ImVec2 at = corner(patch.face);
        const float cell = side / float(1u << patch.level);
        // Hue by level, brightness by how far it has morphed toward its parent; a tile still
        // fading in keeps its hue but washes out, so arrivals stand out from settled patches.
        const ImVec4 tint = ImColor::HSV(std::fmod(patch.level * .17f, 1.f), .70f - .55f * patch.recovery,
                                         .35f + .65f * (1 - patch.morph));
        const ImU32 fill = ImColor(tint);
        for (unsigned q = 0; q < 4; q++) {
            if (!(patch.quadrants >> q & 1))
                continue;
            drawn_quadrants++;
            // t runs up the face, the screen runs down, so the row is mirrored.
            const float x0 = at.x + (patch.x + (q & 1) * .5f) * cell;
            const float y1 = at.y + side - (patch.y + (q >> 1) * .5f) * cell;
            // Two pixels at least: a level-6 quadrant is under one on a face this size, and a
            // map that drops the deep levels is no map of this tree.
            const float half = std::max(cell * .5f, 2.f);
            draw->AddRectFilled({x0, y1 - half}, {x0 + std::max(half - .75f, 1.5f), y1 - .75f}, fill);
        }
    }
    // The camera's own direction, so the drawn set has something to sit against.
    if (map.camera_face < 6) {
        const ImVec2 at = corner(map.camera_face);
        const float cx = at.x + (map.camera_s + 1) * .5f * side;
        const float cy = at.y + side - (map.camera_t + 1) * .5f * side;
        constexpr ImU32 mark = IM_COL32(255, 255, 255, 210);
        draw->AddLine({cx - 5, cy}, {cx + 5, cy}, mark);
        draw->AddLine({cx, cy - 5}, {cx, cy + 5}, mark);
    }
    ImGui::Text("%zu patches, %u quadrants, finest level %u", patches.size(), drawn_quadrants, deepest);
}

void terrain_controls(render::TerrainSettings& terrain, bool& freeze_culling) {
    ImGui::Checkbox("Near tier (N)", &terrain.near_tier);
    ImGui::SameLine();
    ImGui::Checkbox("Wireframe (G)", &terrain.wireframe);
    const char* debug_views[] = {"Shaded",    "Tile coordinate", "Normal",
                                 "Elevation", "Crater shadow",   "Morph and level"};
    int debug = int(terrain.debug);
    if (ImGui::Combo("Patch view (V)", &debug, debug_views, 6))
        terrain.debug = unsigned(debug);
    ImGui::SliderFloat("LOD bias", &terrain.lod_bias, -3.f, 3.f, "%+.2f");
    ImGui::Checkbox("Freeze culling (C)", &freeze_culling);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Holds terrain and belt selection at the current viewpoint.\n"
                          "Move the camera to inspect coverage; streaming can still complete.");
    ImGui::SliderFloat("Tier switch", &terrain.activate_pixels, 50.f, 2400.f, "%.0f px", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Surface detail", &terrain.detail, 0.f, 2.5f, "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Changing surface detail invalidates and regenerates every cached tile.");
}

// A capacity strip ordered by last-use age: its width is slots, not elapsed time.
// Ages use render frames, so the reading remains meaningful while simulation is paused.
void terrain_cache(const render::FrameStats::Terrain& terrain) {
    if (!terrain.slots) {
        ImGui::TextDisabled("Tile cache not initialized");
        return;
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("##terrain-stats", 2, ImGuiTableFlags_SizingStretchSame)) {
        const auto row = [](const char* label, const std::string& value, const std::string& hint,
                            const char* state = "", bool warning = false) {
            ImGui::TableNextColumn();
            const float available = ImGui::GetContentRegionAvail().x;
            ImGui::BeginGroup();
            if (warning)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, .7f, .35f, 1.f});
            ImGui::Text("%s%s", label, warning ? "!" : "");
            // Keep unusually large lifetime counters readable by wrapping within their column.
            if (available >= 48 + ImGui::CalcTextSize(value.c_str()).x)
                ImGui::SameLine(48);
            ImGui::TextUnformatted(value.c_str());
            if (warning)
                ImGui::PopStyleColor();
            ImGui::EndGroup();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s%s%s\n%s", label, *state ? ": " : "", state, hint.c_str());
        };
        row("Cache", std::format("{}/{}/{}", terrain.resident, terrain.pending, terrain.slots_free),
            std::format("Resident / pending / free tiles ({} capacity).\n"
                        "{} requests blocked: no slot can be evicted.\n"
                        "Oldest last use: {} frames; oldest pending: {} frames.\n"
                        "A full cache alone is not evidence of thrashing.\n"
                        "Strip below: recent to cold, then amber pending and gray free.",
                        terrain.slots, terrain.slots_blocked, terrain.oldest_age, terrain.pending_age),
            terrain.slots_blocked ? "blocked" : "", terrain.slots_blocked > 0);
        row("Flow", std::format("{}/{}/{}", terrain.requested, terrain.served, terrain.uploaded),
            std::format("Requested / admitted / uploaded tiles this frame.\n"
                        "{} deferred by throughput; admission budget {}.\n"
                        "A capped flow is normal while loading; it is not cache exhaustion.",
                        terrain.generation_budget_limited, terrain.generation_budget),
            terrain.generation_budget_limited ? "capped" : "");
        row("Queue", std::format("{}/{}/{}", terrain.queued, terrain.rings_free, terrain.rings),
            std::format("Worker jobs / free upload rings / total rings.\n"
                        "{} workers; last generated tile {:.1f} ms.\n"
                        "Oldest pending tile: {} frames. Slow flags a wait over 60 frames.",
                        terrain.workers, terrain.generate_ms, terrain.pending_age),
            terrain.pending_age > 60 ? "slow" : "", terrain.pending_age > 60);
        row("Evict", std::format("{}/{}/{}", terrain.evictions, terrain.evicted_recent, terrain.eviction_total),
            std::format("Evictions / warm evictions this frame / lifetime evictions.\n"
                        "Warm means used fewer than 60 frames ago: a sign of cache competition.\n"
                        "Lifetime warm evictions: {}. Totals survive cache resets.",
                        terrain.warm_eviction_total),
            terrain.evicted_recent ? "warm" : "", terrain.evicted_recent > 0);
        row("Tree", std::format("{}/{}/{}", terrain.nodes, terrain.node_budget, terrain.splits_blocked),
            "Allocated nodes / node budget / blocked splits this frame.\n"
            "The tree budget is independent of tile-cache capacity.",
            terrain.splits_blocked ? "blocked" : "", terrain.splits_blocked > 0);
        row("LOD", std::format("{}/{}/{}", terrain.drawn, terrain.deepest, terrain.behind_one),
            std::format("Drawn patches / finest level / patches over one level behind target.\n"
                        "Mean shortfall: {:.2f} levels; awaiting tiles: {}; balance replacements: {}.\n"
                        "Morph graded / near / far: {} / {} / {}. Range-covered quadrants: {}.\n"
                        "Coarse flags a mean shortfall above one level; loading can cause this.",
                        terrain.behind_mean, terrain.starved, terrain.balanced, terrain.graded, terrain.flat_near,
                        terrain.flat_far, terrain.out_of_range),
            !terrain.active             ? "sphere"
            : terrain.behind_mean > 1.f ? "coarse"
                                        : "",
            terrain.active && terrain.behind_mean > 1.f);
        ImGui::EndTable();
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{std::max(ImGui::GetContentRegionAvail().x, 1.f), 18};
    ImGui::InvisibleButton("##tile-age", size);
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(50, 54, 62, 255));
    float x = origin.x;
    const auto segment = [&](unsigned count, ImU32 color, const char* label) {
        const float end = x + size.x * float(count) / float(terrain.slots);
        draw->AddRectFilled({x, origin.y}, {end, origin.y + size.y}, color);
        if (hovered && ImGui::GetIO().MousePos.x >= x && ImGui::GetIO().MousePos.x < end)
            ImGui::SetTooltip("%s: %u tiles (%.1f%% of capacity)", label, count, 100.f * count / terrain.slots);
        x = end;
    };
    constexpr unsigned age_step = render::FrameStats::Terrain::cache_age_bucket_frames;
    for (unsigned i = 0; i < std::size(terrain.cache_age); ++i) {
        char label[64];
        if (i + 1 == std::size(terrain.cache_age))
            std::snprintf(label, sizeof(label), "Last used %u+ frames ago", i * age_step);
        else
            std::snprintf(label, sizeof(label), "Last used %u-%u frames ago", i * age_step, (i + 1) * age_step - 1);
        segment(terrain.cache_age[i], IM_COL32(70 + i * 12, 185 - i * 11, 220 - i * 7, 255), label);
    }
    segment(terrain.pending, IM_COL32(230, 159, 0, 255), "Pending generation/upload");
    segment(terrain.slots_free, IM_COL32(50, 54, 62, 255), "Free slots");
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
    ImGui::SliderFloat("Follows adaptation", &settings.flare_adaptation, 0.f, 1.f,
                       "%.2f"); // 0 fixed level, 1 dims with the scene
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
    ImGui::SliderFloat("Dead zone", &settings.adapt_deadzone, 0.f, 1.f,
                       "%.2f stops"); // the request must move this far before the exposure follows
    ImGui::SliderFloat("Brighten time", &settings.brighten_seconds, .5f, 15.f, "%.1f s", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Darken time", &settings.darken_seconds, .2f, 5.f, "%.1f s", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Adaptation min", &settings.adapt_min, .05f, 1.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Adaptation max", &settings.adapt_max, 1.f, 32.f, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::EndDisabled();
    // The active curve's exposure trim; each curve keeps its own value.
    ImGui::SliderFloat("Curve trim", &settings.curve_trim[std::min(unsigned(settings.tone_curve), 2u)], .1f, 2.f,
                       "%.2f x");
    // The swapchain's output; the HDR pairs are offered only with the OS presenting in HDR.
    static constexpr const char* outputs[] = {"SDR (8-bit sRGB)", "HDR scRGB (16-bit float)", "HDR10 (10-bit PQ)"};
    combo("Output", settings.hdr_output, outputs);
    if (stats.output.hdr_unsupported)
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
    if (!stats.output.hdr_metadata)
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
            ImGui::SetTooltip("Centre-weighted mean of the histogram below its 99.5th percentile, plus the "
                              "highlight share,\nbefore exposure and bloom; scene-linear luminance, not nits.");
        ImGui::Text("HDR peak (sampled): %.4g", exposure.peak_luminance);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Brightest tap of the meter's four pixel grid, not the brightest full-resolution pixel.");
        ImGui::Text("Adaptation: %.3f x -> %.3f x%s", exposure.adapted, exposure.target,
                    exposure.limited ? " (limited)" : "");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Current automatic multiplier -> clamped target.\nThe histogram completes every 16 "
                              "frames; adaptation follows over time.");
        // The meter's histogram over log2 luminance. Marks: the metered luminance, the
        // key, and the luminances the target and the current adaptation are set for
        // (an exposure e is the key over e on this axis), so their distance from the
        // white mark is the adaptation still to come, in stops.
        float peak_share = 0;
        for (const float share : exposure.histogram)
            peak_share = std::max(peak_share, share);
        const float width = ImGui::GetContentRegionAvail().x, height = 52;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::PlotHistogram("##meter", exposure.histogram, int(render::ExposureStats::histogram_bins), 0, nullptr, 0.f,
                             std::max(peak_share, 1e-6f), ImVec2(width, height));
        const auto stop_x = [&](float luminance) {
            const float stops = (std::log2(std::max(luminance, 1e-9f)) - exposure.stops_min) / exposure.stops_range;
            return origin.x + std::clamp(stops, 0.f, 1.f) * width;
        };
        auto* draw = ImGui::GetWindowDrawList();
        const auto mark = [&](float luminance, ImU32 color, float top, float bottom) {
            const float x = stop_x(luminance);
            draw->AddLine({x, origin.y + top}, {x, origin.y + bottom}, color, 1.5f);
        };
        mark(exposure.luminance, IM_COL32(255, 255, 255, 200), 0, height); // metered
        mark(settings.meter_key, IM_COL32(240, 200, 60, 200), 0, height);  // key
        if (exposure.automatic) {
            mark(settings.meter_key / std::max(exposure.target, 1e-6f), IM_COL32(230, 110, 200, 220), 0,
                 height * .5f); // target, upper half
            mark(settings.meter_key / std::max(exposure.adapted, 1e-6f), IM_COL32(90, 210, 255, 220), height * .5f,
                 height); // current adaptation, lower half
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Share of the meter's taps per bin over %.0f stops from 2^%.0f.\n"
                              "White: the metered luminance.  Yellow: the meter key (maps to 1x).\n"
                              "Magenta (top): the target, %.2f x.  Cyan (bottom): the current adaptation, %.2f x.\n"
                              "The distance from the white mark is the adaptation still to come, in stops.",
                              exposure.stops_range, exposure.stops_min, exposure.target, exposure.adapted);
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
    ImGui::SameLine();
    ImGui::Checkbox("Slow travel (Z)", &app.slow_travel); // a fiftieth of the speed, for a small body
}
void overlay_controls(AppState& app) {
    ImGui::Checkbox("HUD (F1)", &app.overlay);
    ImGui::SameLine();
    if (ImGui::Button("Capture (F10)"))
        request_capture(app);
    ImGui::TextDisabled("Tab hides this panel");
}

} // namespace

void draw_panel(AppState& app, const render::Stats& stats, const SmoothedStats& smoothed, const FrameHistory& history) {
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
        frame_controls(smoothed, history, app.vsync);
        ImGui::PopID();
    }
    if (section("Memory")) { // collapsed by default; the sums are refreshed every frame
        memory_bar(stats.memory);
        ImGui::PopID();
    }
    if (section("Terrain", true)) {
        terrain_controls(app.terrain, app.belt.freeze_culling);
        terrain_cache(stats.frame.terrain);
        patch_map(stats.patch_map);
        ImGui::PopID();
    }
    if (section("Quality")) {
        ImGui::Checkbox("High tier (F2)", &app.high);
        ImGui::SameLine();
        ImGui::TextDisabled(app.high ? "520k rocks" : "280k rocks");
        ImGui::PopID();
    }
    if (section("Anti-aliasing")) {
        anti_aliasing_controls(app.aa);
        ImGui::PopID();
    }
    if (section("Belt")) {
        belt_controls(app.belt, app.belt_dust, smoothed.frame.belt_lod);
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
