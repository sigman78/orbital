#pragma once
#include "scene/geometry.hpp"
#include "scene/terrain_patch.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace space::render {

// Where the tier takes the body over from its sphere levels, in cull_bodies' units;
// TerrainSettings::activate_pixels overrides it per frame.
inline constexpr float TerrainTier_activate_default = 1200;

// The tier's view of one frame: the body relative to the camera, its spin, and
// the projection, in the units cull_bodies uses.
struct TierView {
    Vec3d body_centre;  // camera-relative, units
    double radius = 0;  // the body's, units
    Vec3d axes[3];      // the body's local x, y and z in world
    Vec3d camera_local; // the camera in the body's local frame, radii
    geometry::Frustum frustum;
    float height_pixels = 0, tan_y = 0;
    float activate_pixels = TerrainTier_activate_default; // the switch from the sphere levels
    float lod_bias = 0;                                   // every level's range times 2^bias
    unsigned seam = 0;                                    // TerrainTier::Seam
};

// Cube-sphere selection and tile cache. Distance ranges select patches with hysteresis;
// parents cover unavailable children. Pending and currently used slots cannot be evicted.
// Upload completion establishes residency only for the matching generation stamp.
class TerrainTier {
public:
    static constexpr unsigned slot_count = 1024; // 11 MiB of vertices; a close view holds 400 of them
    static constexpr unsigned generate_per_frame = 8;
    // Sizes are in cull_bodies' units: a projected radius over the half height, twice the pixels.
    static constexpr float error_pixels = 3; // a patch's geometric error on screen: it splits above 1.5 px
    static constexpr float hysteresis = .8f; // the fraction of either the way back
    // Fade new tiles from their parent's shape over a fixed frame count for deterministic captures.
    static constexpr unsigned fade_frames = 15;
    // Worst measured geometric error per level, radii, from 64 random patches per level
    // on the seed-1007 terrain with the Everitt warp, at 32 quads to a tile
    // (2026-09-18). Levels 9..12 extrapolated by the measured ratio of 2.3 per level.
    static constexpr float level_error[] = {
        .018342f,  .0126785f, .00478311f, .00138083f, .000601649f, .000284836f, .000119656f,
        .0000752f, .0000272f, 1.18e-5f,   5.1e-6f,    2.2e-6f,     9.6e-7f,
    };

    struct Generation {
        PatchKey key;
        unsigned slot;
        std::uint32_t stamp; // which assignment of this slot asked for it
    };
    // At grazing angles, a patch can span several morph bands. These policies handle
    // boundaries where the coarse side would otherwise morph away from the fine side.
    enum class Seam : unsigned {
        none,     // as it was: the band is trusted to have cleared the hand-over
        farthest, // a child is drawn only once its whole cap is in range, never part of it
        clamp,    // the coarse patch stops morphing along the sides that meet a finer one
        span,     // a child is drawn only where it is at most one level finer than wanted
        count
    };

    struct Draw {
        PatchKey key;
        unsigned slot;
        unsigned quadrants; // the grid quadrants to draw (bit i: x = i & 1, y = i >> 1); 0xf the whole patch
        // The sides of this patch that meet a finer surface, for Seam::clamp: bit 0
        // low x, 1 high x, 2 low y, 3 high y.
        unsigned finer = 0;
        // Morph floor shared by siblings: 1 on arrival, decreasing to 0 over fade_frames.
        float fade = 0;
    };

    // Selection/cache pressure. starved and splits_blocked indicate limits;
    // out_of_range is ordinary parent coverage.
    struct Pressure {
        unsigned nodes = 0, node_budget = 0, splits_blocked = 0;
        unsigned requested = 0, served = 0;         // tiles asked for, and given a slot
        unsigned evictions = 0, evicted_recent = 0; // resident slots recycled, and those still warm
        unsigned starved = 0, out_of_range = 0;     // quadrants the parent covered, by reason
        unsigned deepest = 0;                       // finest level drawn
        unsigned behind_one = 0, behind_count = 0;  // patches over a level coarser than asked for
        float behind_mean = 0;                      // levels coarser than asked for, averaged
        // Morph variation over each drawn patch's distance bounds; these are estimates.
        // fade_mean is the average arrival morph floor.
        unsigned flat_near = 0, flat_far = 0, graded = 0;
        float fade_mean = 0;
    };
    const Pressure& pressure() const { return pressure_; }

    void update(const TierView& view, unsigned frame, unsigned budget = generate_per_frame);
    void disable(); // the switch off: the tree collapses, the cache stays
    // Discard cached tiles; generation stamps reject results still in flight.
    void invalidate();
    // True once the tier covers the body: the sphere levels draw until then.
    bool active() const { return active_; }
    std::span<const Draw> draws() const { return draws_; } // this frame's patches
    std::span<const Generation> generate() const { return generate_; }
    static constexpr Range<float> full_height_range{MinorPlanetTerrain::height_min, MinorPlanetTerrain::height_max};
    // Accept the uploaded tile and its rendered height range only if this assignment
    // still owns the slot. Unknown ranges retain the conservative full shell.
    bool mark_resident(unsigned slot, PatchKey key, std::uint32_t stamp, Range<float> heights = full_height_range);
    // Only resident tiles supply a tighter range. Unknown tiles retain the full shell.
    Range<float> height_range(PatchKey key) const;
    float range(unsigned level) const { return level < std::size(range_) ? range_[level] : 0; }
    unsigned resident_slot(PatchKey key) const; // the tile's slot, slot_count when absent or pending
    unsigned pending() const;                   // slots handed out whose tile has not arrived
    // Distance bounds for the cap over the supplied height interval. Resident tiles
    // use their own range for drawing decisions; undiscovered descendants keep the
    // full terrain shell. Skirts are excluded, as in the shader's morph distance.
    static double nearest_distance(Vec3d camera_local, const PatchBounds& bounds,
                                   Range<float> heights = full_height_range);
    // The other end of the same cap: what Seam::farthest gates a child on.
    static double farthest_distance(Vec3d camera_local, const PatchBounds& bounds,
                                    Range<float> heights = full_height_range);
    // Fractional desired level and its spread across a patch. Seam::span limits that spread.
    static constexpr double seam_span = 1.0;
    double level_at(double distance) const;
    double level_span(const TierView& view, const PatchBounds& bounds, Range<float> heights = full_height_range) const;
    unsigned resident() const;
    unsigned nodes() const { return unsigned(nodes_.size() - free_blocks_.size() * 4); }

private:
    struct Node {
        PatchKey key;
        PatchBounds bounds;
        std::uint32_t children = 0; // index of the first of four, 0 none (the roots are never children)
    };
    struct Slot {
        PatchKey key;
        unsigned used = 0, resident_frame = 0;
        // Distinguishes repeated assignments of the same key and slot, including detail changes.
        std::uint32_t stamp = 0;
        Range<float> heights = full_height_range;
        bool resident = false;
    };
    struct Request {
        PatchKey key;
        float pixels;
    };
    struct Visibility {
        bool visible = false;
        float pixels = 0;    // the cell's edge on screen, the generation priority
        double distance = 0; // the camera to the nearest point of the patch, radii
    };

    void ensure_roots();
    Visibility visibility(const Node& node, const TierView& view) const;
    void visit(std::uint32_t index, const TierView& view, float fade);
    void collapse(Node& node);
    std::uint32_t allocate_children(const Node& node);
    unsigned slot_of(PatchKey key) const; // slot_count when not in the cache
    void touch(PatchKey key);
    void request(PatchKey key, float pixels);
    void choose_generation(unsigned budget);
    void mark_finer_sides(); // the post-pass Seam::clamp needs: who meets whom

    std::vector<Node> nodes_;
    std::vector<std::uint32_t> free_blocks_;
    std::vector<Slot> slots_;
    std::vector<unsigned> free_slots_;
    std::unordered_map<std::uint32_t, unsigned> slots_by_key_;
    std::vector<Draw> draws_;
    std::vector<Request> requests_;
    std::vector<Generation> generate_;
    float range_[13] = {};
    unsigned frame_ = 0;
    std::uint32_t stamp_ = 0; // the last generation stamp issued, never reused
    Pressure pressure_;
    Seam seam_ = Seam::none;
    bool active_ = false, wanted_ = false;
};

} // namespace space::render
