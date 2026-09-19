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
    // Tree bookkeeping is independent of the tile cache. Both capacity limits can reduce
    // the shared streaming range scale; existing splits retain priority at the node ceiling.
    static constexpr unsigned node_budget = 4096;
    // A generation whose slot is never served would hold its key forever: nothing evicts a
    // pending slot and prepare() only re-requests keys without one. Reclaim it well past any
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
    // Relax a residency-limited range scale back to its requested value over at least this many updates.
    static constexpr unsigned recovery_frames = 15;
    // Seed-1007 calibration against 3D triangles, 32 quads, Everitt warp
    // (`terrain_tests --calibrate`): uniform samples followed by a hill climb.
    // These are estimates, not certified bounds on every patch or intermediate morph.
    static constexpr float level_error[] = {
        .0304741f,   .0192836f, .00591285f,  .00245085f,  .00100089f,  .000457055f, .000173189f,
        8.54412e-5f, 4.493e-5f, 2.27982e-5f, 1.25439e-5f, 4.57292e-6f, 3.38214e-6f,
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
        // Kept from the visit that selected this patch, where its bounds were already at hand:
        // the pressure diagnostics want both distances and would otherwise rebuild the bounds
        // of every drawn patch every frame, which is the dearer half of what they cost.
        float near_distance = 0, far_distance = 0;
    };

    // One level of a drawn patch's provenance, its root first: what each test compared, and by
    // how much it passed. A margin is the room the test had; negative would have culled it, so a
    // drawn patch has none negative and the smallest says which test nearly caught it.
    struct Step {
        PatchKey key;
        unsigned slot = 0;        // slot_count when this node has no tile of its own
        unsigned reach_level = 0; // the level whose tile supplied the reach; its own, or an ancestor's
        Range<float> reach{0, 0};
        // support * d - occluder^2, or infinity where the camera stands inside the occluder and
        // the horizon test does not apply. This is the one that keeps the far side out.
        double horizon_margin = 0;
        double plane_margin = 0; // the tightest frustum plane, and which of the five it was
        unsigned plane = 0;
        double distance = 0, split_range = 0; // what the split decision compared
        bool split = false, drawn = false;
        unsigned quadrants = 0; // the mask it draws, when it is in the draw list
    };
    // The chain of decisions that put a patch in the draw list, root first. Off the hot path: it
    // recomputes what visibility() weighed for a key already chosen, so call it after update()
    // and within the frame whose planes are loaded.
    void explain(PatchKey key, const TierView& view, std::vector<Step>& steps) const;

    // Tree and cache health for the same debugging. `reachable` walks from the six roots, so a
    // gap against `allocated` is a subtree no visit can reach again; `resident_undrawn` is tiles
    // held for a set that is no longer drawn, which is normal for a few and a leak for many.
    struct Audit {
        unsigned reachable = 0, allocated = 0;
        unsigned resident = 0, resident_undrawn = 0;
        unsigned oldest_age = 0; // frames since the least recently used tile was last touched
        // Slots handed out whose tile never came back, and how long the oldest has waited. This
        // is the one part of the tier that clears over many frames rather than at once: nothing
        // evicts a pending slot and prepare() re-requests only keys that have none, so the patch
        // is drawn by its parent, coarse, until the reclaim sweep takes the slot back -- up to
        // pending_timeout plus a sweep cycle, four seconds at 60 Hz. It looks stuck because it
        // is, briefly. A pending_age that keeps climbing past that is a real leak.
        unsigned pending = 0, pending_age = 0;
    };
    Audit audit() const;

    // Per-frame limits and coverage diagnostics; out_of_range is normal coverage.
    struct Pressure {
        unsigned nodes = 0, node_budget = 0, splits_blocked = 0;
        unsigned requested = 0, served = 0;         // tiles asked for, and given a slot
        unsigned evictions = 0, evicted_recent = 0; // resident slots recycled, and those still warm
        unsigned starved = 0, out_of_range = 0;     // requested tiles missing; quadrants covered by range
        unsigned deepest = 0;                       // finest level drawn
        unsigned behind_one = 0, behind_count = 0;  // patches over a level coarser than asked for
        float behind_mean = 0;                      // levels coarser than asked for, averaged
        // Estimated spatial morph variation and residency-limited range scale.
        unsigned flat_near = 0, flat_far = 0, graded = 0;
        float streaming_scale = 0;
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
    // The sphere the horizon test hides patches behind: the shell's floor, never the reference
    // surface, since ground below that surface sets the horizon further out. visibility() has
    // what rests on the choice; the truth test reads it to place its cameras.
    static constexpr double horizon_occluder = 1 + double(MinorPlanetTerrain::height_min);
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
    float morph_start(unsigned level) const {
        // Reserve 40% of the interval after this level's own split threshold before it
        // begins collapsing toward its parent. No fixed ratio between levels is assumed.
        const float end = range(level), next = range(level + 1);
        return next + .4f * (end - next);
    }
    float streaming_scale() const { return streaming_scale_; }
    unsigned resident_slot(PatchKey key) const; // the tile's slot, slot_count when absent or pending
    unsigned pending() const;                   // slots handed out whose tile has not arrived
    unsigned pending_age() const;               // frames the oldest of those has waited, 0 when there are none
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
        unsigned used = 0;
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
    const Node* find(PatchKey key) const; // the node for a key, null where the tree stops short
    // The horizon test over the geometry a draw actually puts on screen: one tile's own heights
    // rather than the reach, which pads for descendants that will not be drawn in its place.
    bool drawn_over_horizon(const PatchBounds& bounds, Range<float> heights) const;
    Visibility visibility(const Node& node, const TierView& view) const;
    double prepare(std::uint32_t index, const TierView& view, const Visibility& seen);
    void visit(std::uint32_t index, const TierView& view, const Visibility& seen);
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
    float range_[std::size(level_error) + 1] = {};
    float requested_range_[std::size(level_error) + 1] = {};
    float streaming_scale_ = 0;
    LocalPlane planes_[5] = {}; // near and the four sides; the frustum's sixth is the far plane, not tested
    // The camera in the body's frame, resolved once an update: every node tested the horizon
    // against it, and every one of them was taking the same square root to do so.
    Vec3d eye_{0, 0, 1};
    double camera_distance_ = 0;
    unsigned frame_ = 0;
    unsigned sweep_cursor_ = 0; // where the pending-slot reclaim sweep resumes
    std::uint32_t stamp_ = 0;   // the last generation stamp issued, never reused
    Pressure pressure_;
    bool active_ = false, wanted_ = false;
};

} // namespace space::render
