#pragma once
#include "scene/geometry.hpp"
#include "scene/terrain_patch.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace space::render {

// The tier's view of one frame: the body relative to the camera, its spin, and
// the projection, in the units cull_bodies uses.
struct TierView {
    Vec3d body_centre;  // camera-relative, units
    double radius = 0;  // the body's, units
    Vec3d axes[3];      // the body's local x, y and z in world
    Vec3d camera_local; // the camera in the body's local frame, radii
    geometry::Frustum frustum;
    float height_pixels = 0, tan_y = 0;
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
    static constexpr float activate_pixels = 1200; // the body's, where the finest sphere level runs out
    static constexpr float error_pixels = 3;       // a patch's geometric error on screen: it splits above 1.5 px
    static constexpr float hysteresis = .8f;       // the fraction of either the way back
    // Worst measured geometric error per level, radii, from 64 random patches per level
    // on the seed-1007 terrain with the Everitt warp (2026-09-18). Levels 9..12
    // extrapolated by the measured ratio of 2.5 per level.
    static constexpr float level_error[] = {
        .01458f,   .00478f,   .00215f, .000761f, .000300f, .000136f, .0000862f,
        .0000356f, .0000145f, 5.8e-6f, 2.3e-6f,  9.3e-7f,  3.7e-7f,
    };

    struct Generation {
        PatchKey key;
        unsigned slot;
    };
    struct Draw {
        PatchKey key;
        unsigned slot;
        unsigned quadrants; // the grid quadrants to draw (bit i: x = i & 1, y = i >> 1); 0xf the whole patch
    };

    void update(const TierView& view, unsigned frame, unsigned budget = generate_per_frame);
    void disable(); // the switch off: the tree collapses, the cache stays
    // True once the tier covers the body: the sphere levels draw until then.
    bool active() const { return active_; }
    std::span<const Draw> draws() const { return draws_; } // this frame's patches
    std::span<const Generation> generate() const { return generate_; }
    // Marks a slot resident if it still holds key; false if it was recycled meanwhile.
    bool mark_resident(unsigned slot, PatchKey key);
    float range(unsigned level) const { return level < std::size(range_) ? range_[level] : 0; }
    unsigned resident_slot(PatchKey key) const; // the tile's slot, slot_count when absent or pending
    unsigned pending() const;                   // slots handed out whose tile has not arrived
    // The closest the camera (body frame, radii) can be to any point of the patch: its cap
    // over the shell between the terrain's lowest and highest heights. The shader morphs by
    // the vertex's own distance, which is never less, so an unsplit neighbour's edge is
    // beyond the child's range and the child is fully morphed there.
    static double nearest_distance(Vec3d camera_local, const PatchBounds& bounds);
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
        bool resident = false;
    };
    struct Request {
        PatchKey key;
        float pixels;
    };
    struct Visibility {
        bool visible = false;
        float pixels = 0; // the cell's edge on screen, the generation priority
    };

    void ensure_roots();
    Visibility visibility(const Node& node, const TierView& view) const;
    void visit(std::uint32_t index, const TierView& view);
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
    unsigned frame_ = 0;
    bool active_ = false, wanted_ = false;
};

} // namespace space::render
