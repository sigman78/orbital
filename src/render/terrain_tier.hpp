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

// The near tier's CPU side: which patches of the cube sphere to draw this frame
// and which to generate for the next, up to a per-frame budget, over a fixed
// pool of tile slots kept as a cache by patch (least recently used resident
// slot out; a pending one stays put until it lands). The quadtree splits by
// per-level distance ranges derived from the error table, with hysteresis; a
// patch whose visible children are not all resident draws itself. Slots start
// non-resident and the caller marks a slot resident by slot and key, once the
// tile upload for that key completes; a slot recycled meanwhile is left alone.
class TerrainTier {
public:
    static constexpr unsigned slot_count = 1024; // 11 MiB of vertices; a close view holds 400 of them
    static constexpr unsigned generate_per_frame = 8;
    // Sizes are in cull_bodies' units: a projected radius over the half height, twice the pixels.
    static constexpr float error_pixels = 3; // a patch's geometric error on screen: it splits above 1.5 px
    static constexpr float hysteresis = .8f; // the fraction of either the way back
    // A tile just arrived is drawn morphed to its parent's shape and relaxes to its
    // own over this many frames, so detail fades in where it would otherwise pop.
    // Counted in frames, not seconds, so a capture is the same every run.
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
    // What to do where a patch meets a finer one. A patch is drawn out to its own
    // range and morphs over the outer part of it, and from a grazing view one patch
    // spans distances from under the camera to the horizon, so a border with a finer
    // neighbour can sit deep inside the coarse patch's morph band. The finer side is
    // fully morphed to the coarse patch's own shape there, and the coarse patch is
    // most of the way to its parent's: a seam, which a view from above never shows
    // because the patch subtends too little distance for the two to part.
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
        // A floor under the vertex shader's morph, 1 the moment the patch is drawn
        // for the first time and 0 once it has settled. The four children of one
        // split share it, so they agree with each other while they fade in.
        float fade = 0;
    };

    // Everything the tier can run out of in a frame, so a flight can be flown and the
    // log read back afterwards. The two that change what is drawn are `starved`, a
    // quadrant the parent covered because its child had no tile, and `splits_blocked`,
    // a node that wanted to split and was refused by the node budget; `out_of_range`
    // is the same cover for the ordinary reason and is not a limit at all.
    struct Pressure {
        unsigned nodes = 0, node_budget = 0, splits_blocked = 0;
        unsigned requested = 0, served = 0;         // tiles asked for, and given a slot
        unsigned evictions = 0, evicted_recent = 0; // resident slots recycled, and those still warm
        unsigned starved = 0, out_of_range = 0;     // quadrants the parent covered, by reason
        unsigned deepest = 0;                       // finest level drawn
        unsigned behind_one = 0, behind_count = 0;  // patches over a level coarser than asked for
        float behind_mean = 0;                      // levels coarser than asked for, averaged
        // Whether the morph is doing anything. A patch whose morph reads the same at
        // both ends of it has every vertex moving together, so it switches where it
        // should blend, which is what chunked LOD does; graded is the share carrying a
        // gradient across itself. fade_mean is the temporal floor, which hides the rest
        // of it while tiles are still arriving.
        unsigned flat_near = 0, flat_far = 0, graded = 0;
        float fade_mean = 0;
    };
    const Pressure& pressure() const { return pressure_; }

    void update(const TierView& view, unsigned frame, unsigned budget = generate_per_frame);
    void disable(); // the switch off: the tree collapses, the cache stays
    // Drops every tile: what a change to how they are generated needs. Tiles in
    // flight land on a recycled slot and are refused, as they are after an eviction.
    void invalidate();
    // True once the tier covers the body: the sphere levels draw until then.
    bool active() const { return active_; }
    std::span<const Draw> draws() const { return draws_; } // this frame's patches
    std::span<const Generation> generate() const { return generate_; }
    // Marks a slot resident if it still holds key; false if it was recycled meanwhile.
    bool mark_resident(unsigned slot, PatchKey key, std::uint32_t stamp);
    float range(unsigned level) const { return level < std::size(range_) ? range_[level] : 0; }
    unsigned resident_slot(PatchKey key) const; // the tile's slot, slot_count when absent or pending
    unsigned pending() const;                   // slots handed out whose tile has not arrived
    // The closest the camera (body frame, radii) can be to any point of the patch: its cap
    // over the shell between the terrain's lowest and highest heights. The shader morphs by
    // the vertex's own distance, which is never less, so an unsplit neighbour's edge is
    // beyond the child's range and the child is fully morphed there.
    static double nearest_distance(Vec3d camera_local, const PatchBounds& bounds);
    // The other end of the same cap: what Seam::farthest gates a child on.
    static double farthest_distance(Vec3d camera_local, const PatchBounds& bounds);
    // The fractional level the screen error asks for at a distance, and how far that
    // varies across a patch. Seam::span splits while the spread is over seam_span,
    // since the morph can only carry one level of it.
    static constexpr double seam_span = 1.0;
    double level_at(double distance) const;
    double level_span(const TierView& view, const PatchBounds& bounds) const;
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
        // Slot and key do not name a generation: a patch evicted and asked for again,
        // or asked for again at another detail setting, gets both back. The stamp is
        // issued once per assignment and never reissued, so a result that was in
        // flight across the change is told apart from the one that replaced it.
        std::uint32_t stamp = 0;
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
