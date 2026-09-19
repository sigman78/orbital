#pragma once
#include "scene/geometry.hpp"
#include "scene/terrain_patch.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace space::render {

// Default near-tier activation threshold in cull_bodies' units; overridable per frame.
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
};

// Distance-based quadtree selection with parent fallback and a generation-checked tile cache.
class TerrainTier {
public:
    static constexpr unsigned slot_count = 1024; // 35 MiB of tiles; a close view holds 400 of them
    // Tree size, deliberately not the slot count: a node is 64 bytes and only drawn patches
    // need slots, so the tile cache's LRU is the real limit. Demand peaks near 1400 at bias 2,
    // and a node blocked here never recovers, since a split already made is grandfathered.
    static constexpr unsigned node_budget = 4096;
    // A generation whose slot is never served would hold its key forever: nothing evicts a
    // pending slot and visit() only re-requests keys without one. Reclaim it well past any
    // real generation, which takes milliseconds. The sweep walks a window of slots per frame,
    // so the whole cache is covered every slot_count / sweep_window frames.
    static constexpr unsigned pending_timeout = 240;
    static constexpr unsigned sweep_window = 64;
    // Culling a patch against its own tile would drop relief only its descendants reveal, so
    // the interval is padded by how far they reach outside it. `terrain_tests --calibrate`
    // measures one step, parent to child; the pad must cover the **whole subtree**, so these
    // are the running sums of those steps from each level down, at twice for headroom. One
    // step is not enough: at level 6 it is 0.000057 against a subtree's 0.000128, and the gap
    // culls patches whose children stand higher than they do.
    static constexpr float descendant_relief[] = {
        .0286f, .0136f, .0061f, .0036f, .0014f, .00056f, .00026f, .00015f, .000055f, .000016f, .000004f, 2e-6f, 2e-6f,
    };
    static constexpr float relief_margin(unsigned level) {
        return level < std::size(descendant_relief) ? descendant_relief[level] : 0.f;
    }
    static constexpr unsigned generate_per_frame = 8;
    // Sizes are in cull_bodies' units: a projected radius over the half height, twice the pixels.
    static constexpr float error_pixels = 3; // a patch's geometric error on screen: it splits above 1.5 px
    static constexpr float hysteresis = .8f; // the fraction of either the way back
    // Fade new tiles from their parent's shape over a fixed frame count for deterministic captures.
    static constexpr unsigned fade_frames = 15;
    // Seed-1007 calibration, 32 quads, Everitt warp (`terrain_tests --calibrate`): a uniform
    // sample per level, then a hill climb around its worst patch, since relief clusters and a
    // uniform sample misses it. Levels 9..12 previously extrapolated at about half the true
    // error. One method for the whole table: test_morph_bands constrains adjacent ratios, so a
    // level measured more thoroughly than its neighbour breaks the band it hands over in.
    static constexpr float level_error[] = {
        .018342f,     .0126785f,    .00478311f,   .00225514f,  .000698864f, .000284836f, .000153035f,
        .0000752807f, .0000374019f, .0000214279f, 9.11951e-6f, 4.02331e-6f, 1.99676e-6f,
    };

    struct Generation {
        PatchKey key;
        unsigned slot;
        std::uint32_t stamp; // which assignment of this slot asked for it
    };
    struct Draw {
        PatchKey key;
        unsigned slot;
        unsigned quadrants; // the grid quadrants to draw (bit i: x = i & 1, y = i >> 1); 0xf the whole patch
        // Morph floor shared by siblings: 1 on arrival, decreasing to 0 over fade_frames.
        float fade = 0;
        // Kept from the visit that selected this patch, where its bounds were already at hand:
        // the pressure diagnostics want both distances and would otherwise rebuild the bounds
        // of every drawn patch every frame, which is the dearer half of what they cost.
        float near_distance = 0, far_distance = 0;
    };

    // Per-frame limits and coverage diagnostics; out_of_range is normal coverage.
    struct Pressure {
        unsigned nodes = 0, node_budget = 0, splits_blocked = 0;
        unsigned requested = 0, served = 0;         // tiles asked for, and given a slot
        unsigned evictions = 0, evicted_recent = 0; // resident slots recycled, and those still warm
        unsigned starved = 0, out_of_range = 0;     // quadrants the parent covered, by reason
        unsigned deepest = 0;                       // finest level drawn
        unsigned behind_one = 0, behind_count = 0;  // patches over a level coarser than asked for
        float behind_mean = 0;                      // levels coarser than asked for, averaged
        // Estimated spatial morph variation and mean arrival fade.
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
    // Accept uploaded contents and height bounds only for the current slot assignment.
    bool mark_resident(unsigned slot, PatchKey key, std::uint32_t stamp, Range<float> heights = full_height_range);
    // Hand back a slot from generate() the caller could not serve, so the patch is asked for
    // again. The stamp rejects a slot already reassigned, as mark_resident does.
    void release(unsigned slot, PatchKey key, std::uint32_t stamp);
    // Only resident tiles supply a tighter range. Unknown tiles retain the full shell.
    Range<float> height_range(PatchKey key) const;
    // The interval culling and distances use: height_range padded by relief_margin, inherited
    // from the nearest resident ancestor while a node has no tile of its own.
    Range<float> reach_of(PatchKey key) const;
    float range(unsigned level) const { return level < std::size(range_) ? range_[level] : 0; }
    unsigned resident_slot(PatchKey key) const; // the tile's slot, slot_count when absent or pending
    unsigned pending() const;                   // slots handed out whose tile has not arrived
    // Cap distance over the supplied height interval, excluding skirts as in the shader.
    static double nearest_distance(Vec3d camera_local, const PatchBounds& bounds,
                                   Range<float> heights = full_height_range);
    static double farthest_distance(Vec3d camera_local, const PatchBounds& bounds,
                                    Range<float> heights = full_height_range);
    // The fractional level the screen error asks for at a distance.
    double level_at(double distance) const;
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
        unsigned assigned_frame = 0; // when the generation went out; `used` tracks visits instead
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
    // The frustum carried into the body's frame and scaled to radii, rebuilt each update:
    // hundreds of nodes test against the same planes, and none of them should redo this.
    struct LocalPlane {
        Vec3d normal;
        double offset = 0; // a patch is outside when its support falls below -offset
    };

    void ensure_roots();
    Visibility visibility(const Node& node, const TierView& view) const;
    void visit(std::uint32_t index, const TierView& view, const Visibility& seen, float fade);
    void collapse(Node& node);
    std::uint32_t allocate_children(const Node& node);
    unsigned slot_of(PatchKey key) const; // slot_count when not in the cache
    void touch(PatchKey key);
    void request(PatchKey key, float pixels);
    void choose_generation(unsigned budget);

    std::vector<Node> nodes_;
    std::vector<std::uint32_t> free_blocks_;
    std::vector<Slot> slots_;
    std::vector<unsigned> free_slots_;
    std::unordered_map<std::uint32_t, unsigned> slots_by_key_;
    std::vector<Draw> draws_;
    std::vector<Request> requests_;
    std::vector<Generation> generate_;
    float range_[13] = {};
    LocalPlane planes_[5] = {}; // near and the four sides; the frustum's sixth is the far plane, not tested
    unsigned frame_ = 0;
    unsigned sweep_cursor_ = 0; // where the pending-slot reclaim sweep resumes
    std::uint32_t stamp_ = 0;   // the last generation stamp issued, never reused
    Pressure pressure_;
    bool active_ = false, wanted_ = false;
};

} // namespace space::render
