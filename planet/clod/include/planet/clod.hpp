#pragma once
#include "planet/allocator.hpp"
#include "planet/body.hpp"
#include "planet/clod_health.hpp"
#include "planet/patch.hpp"
#include "planet/types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace planet {

// Default near-tier activation threshold in cull_bodies' units; overridable per frame.
inline constexpr float clod_activate_default = 1200;

// A world-space clipping plane: dot(normal, p) + distance >= 0 is inside.
struct ViewPlane {
    Vec3f normal;
    float distance = 0;
};

// The tier's view of one frame: the body relative to the camera, its spin, and
// the projection, in the units cull_bodies uses.
struct ClodView {
    Vec3d body_centre;   // camera-relative, units
    double radius = 0;   // the body's, units
    Vec3d axes[3];       // the body's local x, y and z in world
    Vec3d camera_local;  // the camera in the body's local frame, radii
    ViewPlane planes[5]; // near and the four sides; the far plane is never tested
    float height_pixels = 0, tan_y = 0;
    float activate_pixels = clod_activate_default; // the switch from the sphere levels
    float lod_bias = 0;                            // every level's range times 2^bias
    // Nominal projected grid interval, in actual pixels; 0 retains error-only selection.
    float mesh_pixels = 0;
};

// What a Clod is built from: the body's shell and level tables come from whatever supplies
// its tiles, the cache capacity from the arrays the host allocated for them.
struct ClodConfig {
    BodyShape shape;
    LevelTable levels;
    unsigned cache_capacity = 1024;
    const Allocator* allocator = nullptr; // null: default_allocator()
};

// Distance-based quadtree selection with parent fallback and a generation-checked tile cache.
class Clod {
public:
    // Keep absence independent of the capacity so callers cannot mistake a valid extra slot
    // for no tile. The renderer converts absence to its shader's parent-layer sentinel.
    static constexpr unsigned no_slot = std::numeric_limits<unsigned>::max();
    static constexpr unsigned cache_age_buckets = 8;
    static constexpr unsigned cache_age_bucket_frames = 30;
    // Tree bookkeeping is independent of the tile cache. Both capacity limits can reduce
    // local refinement; existing splits retain priority at the node ceiling.
    static constexpr unsigned node_budget = 4096;
    // A generation whose slot is never served would hold its key forever: nothing evicts a
    // pending slot and prepare() only re-requests keys without one. Reclaim it well past any
    // real generation, which takes milliseconds. The sweep walks a window of slots per frame,
    // so the whole cache is covered every capacity / sweep_window frames.
    static constexpr unsigned pending_timeout = 240;
    static constexpr unsigned sweep_window = 64;
    static constexpr unsigned generate_per_frame = 8;
    // Sizes are in cull_bodies' units: a projected radius over the half height, twice the pixels.
    static constexpr float error_pixels = 10; // 5 screen pixels: preserves the former default workload
    static constexpr float hysteresis = .8f;  // the fraction of either the way back
    // Local arrival transition; it never changes another patch's distance ranges.
    static constexpr unsigned recovery_frames = 15;

    struct Generation {
        PatchKey key;
        unsigned slot;
        std::uint32_t stamp; // which assignment of this slot asked for it
    };
    struct Draw {
        PatchKey key;
        unsigned slot;
        unsigned quadrants;        // the grid quadrants to draw (bit i: x = i & 1, y = i >> 1); 0xf the whole patch
        unsigned coarse_edges = 0; // four side bits per quadrant: collapse this edge to a coarser neighbour
        unsigned fine_edges = 0;   // keep this edge unmorphed beside a finer neighbour or another cube face
        float fade = 0;
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
        unsigned slot = no_slot;  // no_slot when this node has no tile of its own
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
    void explain(PatchKey key, const ClodView& view, std::vector<Step>& steps) const;

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

    // A compact cache snapshot for the terrain panel and deterministic pressure sweeps. Age
    // buckets run recent to cold in cache_age_bucket_frames-wide linear bands; the last is open.
    struct CacheStats {
        unsigned capacity = 0, free = 0, resident = 0, pending = 0, oldest_age = 0, pending_age = 0;
        std::array<unsigned, cache_age_buckets> age{};
    };
    CacheStats cache_stats(unsigned frame) const;

    // Per-frame limits and coverage diagnostics; out_of_range is normal coverage.
    struct Pressure {
        unsigned cache_capacity = 0, cache_free = 0, cache_resident = 0, cache_pending = 0, cache_oldest_age = 0,
                 cache_pending_age = 0;
        // Resident age, recent to cold: [0,30), [30,60), ... [180,210), [210,+) frames.
        std::array<unsigned, cache_age_buckets> cache_age{};
        unsigned nodes = 0, node_budget = 0, splits_blocked = 0;
        unsigned requested = 0, served = 0;         // tiles asked for, and given a slot
        unsigned evictions = 0, evicted_recent = 0; // resident slots recycled, and those still warm
        std::uint64_t eviction_total = 0, warm_eviction_total = 0;
        // Requests left unassigned this update, split by the resource that stopped admission.
        // `generation_budget` is min(the caller's staging-ring budget, generate_per_frame).
        unsigned generation_budget = 0, generation_budget_limited = 0, slots_blocked = 0;
        unsigned starved = 0, out_of_range = 0;    // requested tiles missing; quadrants covered by range
        unsigned deepest = 0;                      // finest level drawn
        unsigned behind_one = 0, behind_count = 0; // patches over a level coarser than asked for
        float behind_mean = 0;                     // levels coarser than asked for, averaged
        // Estimated interior morph variation and local balancing work.
        unsigned flat_near = 0, flat_far = 0, graded = 0;
        unsigned balanced = 0;
    };
    const Pressure& pressure() const { return pressure_; }
    const ClodHealth& health() const { return health_; }

    explicit Clod(const ClodConfig& config);
    void update(const ClodView& view, unsigned frame, unsigned budget = generate_per_frame);
    void disable(); // the switch off: the tree collapses, the cache stays
    // Discard cached tiles; generation stamps reject results still in flight.
    void invalidate();
    // True once the tier covers the body: the sphere levels draw until then.
    bool active() const { return active_; }
    std::span<const Draw> draws() const { return draws_; } // this frame's patches
    std::span<const Generation> generate() const { return generate_; }
    const BodyShape& shape() const { return shape_; }
    const LevelTable& levels() const { return levels_; }
    // The sphere the horizon test hides patches behind; visibility() has what rests on the
    // choice, and the truth test reads it to place its cameras.
    double occluder() const { return shape_.occluder(); }
    // The pad culling a patch against its own tile needs for the relief its descendants reveal.
    float relief_margin(unsigned level) const { return levels_.relief_at(level); }
    // Accept uploaded contents and height bounds only for the current slot assignment; pass the
    // shell when the tile's own range is unknown.
    bool mark_resident(unsigned slot, PatchKey key, std::uint32_t stamp, Range<float> heights);
    // Hand back a slot from generate() the caller could not serve, so the patch is asked for
    // again. The stamp rejects a slot already reassigned, as mark_resident does.
    void release(unsigned slot, PatchKey key, std::uint32_t stamp);
    // Only resident tiles supply a tighter range. Unknown tiles retain the full shell.
    Range<float> height_range(PatchKey key) const;
    // The interval culling and distances use: measured heights padded by relief_margin,
    // retained in live nodes after eviction, or inherited from the nearest measured ancestor.
    Range<float> reach_of(PatchKey key) const;
    float range(unsigned level) const { return level < std::size(range_) ? range_[level] : 0; }
    float morph_start(unsigned level) const {
        // Reserve 40% of the interval after this level's own split threshold before it
        // begins collapsing toward its parent. No fixed ratio between levels is assumed.
        const float end = range(level), next = range(level + 1);
        return next + .4f * (end - next);
    }
    unsigned resident_slot(PatchKey key) const; // the tile's slot, no_slot when absent or pending
    unsigned pending() const;                   // slots handed out whose tile has not arrived
    unsigned pending_age() const;               // frames the oldest of those has waited, 0 when there are none
    // Nearest distance to the spherical cell over the supplied height interval, excluding skirts.
    static double nearest_distance(Vec3d camera_local, const PatchBounds& bounds, Range<float> heights);
    static double farthest_distance(Vec3d camera_local, const PatchBounds& bounds, Range<float> heights);
    // The fractional level the screen error asks for at a distance.
    double level_at(double distance) const;
    unsigned resident() const;
    unsigned capacity() const { return capacity_; }
    unsigned nodes() const { return unsigned(nodes_.size() - free_blocks_.size() * 4); }

private:
    struct Node {
        PatchKey key;
        PatchBounds bounds;
        std::uint32_t children = 0; // index of the first of four, 0 none (the roots are never children)
        // Bounds are knowledge about the terrain, not ownership of a GPU tile. Retain them
        // after eviction so a rejected tile does not become eligible just by losing its slot.
        Range<float> heights; // the shell until measured
        bool measured = false;
    };
    struct Slot {
        PatchKey key;
        unsigned used = 0;
        unsigned guard_frame = 0;    // speculative neighbour; visible requests may evict it
        unsigned fade_update = 0;    // first display of this assignment; visibility never restarts the fade
        unsigned assigned_frame = 0; // when the generation went out; `used` tracks visits instead
        // Distinguishes repeated assignments of the same key and slot, including detail changes.
        std::uint32_t stamp = 0;
        Range<float> heights; // the shell until the tile's own range arrives
        bool resident = false;
    };
    struct Request {
        PatchKey key;
        float pixels;
        bool prefetch = false;
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
    // Visibility of geometry actually submitted: one tile's own heights, without padding
    // for descendants that will not be drawn in its place.
    bool drawn_visible(const PatchBounds& bounds, Range<float> heights) const;
    Visibility visibility(const Node& node, const ClodView& view) const;
    void prepare(std::uint32_t index, const ClodView& view, const Visibility& seen);
    void visit(std::uint32_t index, const ClodView& view, const Visibility& seen);
    void stitch(const ClodView& view);
    void cull_draw_quadrants();
    void monitor_coverage(std::span<const Draw> before_balance);
    void collapse(Node& node);
    std::uint32_t allocate_children(const Node& node);
    unsigned slot_of(PatchKey key) const; // no_slot when not in the cache
    void touch(PatchKey key);
    void request(PatchKey key, float pixels, bool prefetch = false);
    void prefetch_neighbours();
    void choose_generation(unsigned budget);

    std::vector<Node> nodes_;
    std::vector<std::uint32_t> free_blocks_;
    std::vector<Slot> slots_;
    std::vector<unsigned> free_slots_;
    std::unordered_map<std::uint32_t, unsigned> slots_by_key_;
    std::vector<Draw> draws_;
    std::vector<Draw> previous_draws_;
    std::vector<Request> requests_;
    std::vector<Generation> generate_;
    float range_[LevelTable::count + 1] = {};
    LocalPlane planes_[5] = {}; // near and the four sides; the frustum's sixth is the far plane, not tested
    // The camera in the body's frame, resolved once an update: every node tested the horizon
    // against it, and every one of them was taking the same square root to do so.
    Vec3d eye_{0, 0, 1};
    double camera_distance_ = 0;
    BodyShape shape_;
    LevelTable levels_;
    const Allocator* allocator_; // owns nothing yet: the containers above move onto it in phase 2
    unsigned frame_ = 0;
    unsigned update_ = 0;
    unsigned capacity_;
    unsigned sweep_cursor_ = 0; // where the pending-slot reclaim sweep resumes
    std::uint32_t stamp_ = 0;   // the last generation stamp issued, never reused
    std::uint64_t eviction_total_ = 0, warm_eviction_total_ = 0;
    Pressure pressure_;
    ClodHealth health_;
    bool active_ = false, wanted_ = false;
};

} // namespace planet
